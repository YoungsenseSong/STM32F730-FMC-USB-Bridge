"""Long-test and display the real WCH UBR1 stream from FPGA/FSMC."""

from __future__ import annotations

import argparse
import binascii
import json
import struct
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports


VID_PID = (0x1A86, 0xFE0C)
USB_HEADER_BYTES = 24
BLOCK_HEADER_BYTES = 64
RECORD_BYTES = 248
BLOCK_MAGIC = 0x31425046
RECORD_MAGIC = 0x3146524E
USB_MAGIC = 0x31524255


def auto_port() -> str:
    ports = [p.device for p in list_ports.comports()
             if (p.vid, p.pid) == VID_PID]
    if len(ports) != 1:
        raise RuntimeError(f"expected one WCH CDC port {VID_PID!r}; found {ports}")
    return ports[0]


def crc16(data: bytes) -> int:
    return binascii.crc_hqx(data, 0xFFFF)


def read_exact(port: serial.Serial, count: int, deadline: float) -> bytes:
    result = bytearray()
    while len(result) < count:
        if time.monotonic() >= deadline:
            raise TimeoutError(f"read timeout {len(result)}/{count} bytes")
        result.extend(port.read(count - len(result)))
    return bytes(result)


def seek_magic(port: serial.Serial, deadline: float) -> None:
    window = bytearray()
    while bytes(window) != b"UBR1":
        if time.monotonic() >= deadline:
            raise TimeoutError("no UBR1 frame before idle timeout")
        window.extend(port.read(1))
        if len(window) > 4:
            del window[:-4]


def validate_block(payload: bytes) -> tuple[int, int, int, tuple[int, ...], int, tuple[int, ...]]:
    if len(payload) < BLOCK_HEADER_BYTES:
        raise ValueError("block shorter than header")
    (magic, version, header_bytes, block_sequence, _sync_epoch,
     payload_bytes, channel_mask, _first_tick, _last_tick,
     count0, count1, count2, count3, flags, payload_crc) = struct.unpack_from(
         "<IHHIIIIQQIIIIII", payload, 0)
    if (magic, version, header_bytes) != (BLOCK_MAGIC, 1, BLOCK_HEADER_BYTES):
        raise ValueError(f"bad block header {magic:08x}/{version}/{header_bytes}")
    if payload_bytes != len(payload) - BLOCK_HEADER_BYTES:
        raise ValueError("block length does not match USB payload")
    if (flags & ~0x8) != 0:
        raise ValueError(f"unsupported block flags 0x{flags:x}")
    counts = (count0, count1, count2, count3)
    record_count = sum(counts)
    if payload_bytes != record_count * RECORD_BYTES:
        raise ValueError("record count does not match block length")
    if (binascii.crc32(payload[BLOCK_HEADER_BYTES:]) & 0xFFFFFFFF) != payload_crc:
        raise ValueError("block payload CRC mismatch")
    offset = BLOCK_HEADER_BYTES
    for record_index in range(record_count):
        record = payload[offset:offset + RECORD_BYTES]
        if len(record) != RECORD_BYTES or struct.unpack_from("<I", record, 0)[0] != RECORD_MAGIC:
            raise ValueError(f"bad record magic at index {record_index}")
        if record[4] != 1 or struct.unpack_from("<H", record, 6)[0] != 1:
            raise ValueError(f"bad record version/type at index {record_index}")
        if crc16(record[:38]) != struct.unpack_from("<H", record, 38)[0]:
            raise ValueError(f"record header CRC mismatch at index {record_index}")
        if (binascii.crc32(record[40:244]) & 0xFFFFFFFF) != struct.unpack_from("<I", record, 244)[0]:
            raise ValueError(f"record payload CRC mismatch at index {record_index}")
        offset += RECORD_BYTES
    if offset != len(payload):
        raise ValueError("block record area has trailing bytes")
    samples: tuple[int, ...] = ()
    if record_count:
        first_record = payload[BLOCK_HEADER_BYTES:BLOCK_HEADER_BYTES + RECORD_BYTES]
        samples = struct.unpack_from("<12h", first_record, 52)
    return block_sequence, record_count, flags, samples, channel_mask, counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="defaults to auto-detecting 1A86:FE0C")
    parser.add_argument("--seconds", type=float, default=600.0)
    parser.add_argument("--frames", type=int, default=0,
                        help="stop after this many frames; 0 means duration only")
    parser.add_argument("--idle-timeout", type=float, default=15.0)
    parser.add_argument("--show-every", type=int, default=1)
    parser.add_argument("--evidence-dir", type=Path,
                        default=Path("evidence") / "real_stream")
    args = parser.parse_args()
    if args.seconds <= 0 or args.frames < 0 or args.idle_timeout <= 0 or args.show_every < 1:
        parser.error("seconds, idle-timeout and show-every must be positive; frames must be nonnegative")

    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    raw_path = args.evidence_dir / f"real_stream_{stamp}.bin"
    json_path = args.evidence_dir / f"real_stream_{stamp}.json"
    result = {
        "status": "FAIL", "stage": "start", "timestamp": datetime.now().astimezone().isoformat(),
        "parameters": vars(args) | {"evidence_dir": str(args.evidence_dir)},
        "validated_frames": 0, "validated_bytes": 0, "idle_events": 0,
        "sequence_gaps": [], "errors": [], "raw_file": str(raw_path),
    }
    started = time.monotonic()
    raw = bytearray()
    previous_packet = None
    previous_block = None
    try:
        result["stage"] = "open_port"
        port_name = args.port or auto_port()
        result["port"] = port_name
        with serial.Serial(port_name, 115200, timeout=0.1, write_timeout=args.idle_timeout) as port:
            port.dtr = True
            deadline = started + args.seconds
            while time.monotonic() < deadline and (not args.frames or result["validated_frames"] < args.frames):
                result["stage"] = "seek_frame"
                try:
                    seek_magic(port, min(deadline, time.monotonic() + args.idle_timeout))
                except TimeoutError:
                    result["idle_events"] += 1
                    print(f"REAL idle={result['idle_events']} frames={result['validated_frames']} bytes={len(raw)}", flush=True)
                    continue
                header = b"UBR1" + read_exact(port, USB_HEADER_BYTES - 4,
                                               min(deadline, time.monotonic() + args.idle_timeout))
                magic, version, kind, packet, block, length, frame_crc = struct.unpack(
                    "<IHHIIII", header)
                if (magic, version, kind) != (USB_MAGIC, 1, 1):
                    raise ValueError("invalid UBR1 header")
                payload = read_exact(port, length,
                                     min(deadline, time.monotonic() + args.idle_timeout))
                raw.extend(header + payload)
                if (binascii.crc32(payload) & 0xFFFFFFFF) != frame_crc:
                    raise ValueError(f"USB payload CRC mismatch at packet {packet}")
                block_sequence, record_count, flags, samples, channel_mask, channel_counts = validate_block(payload)
                if previous_packet is not None and packet != ((previous_packet + 1) & 0xFFFFFFFF):
                    result["sequence_gaps"].append({"expected": previous_packet + 1, "actual": packet})
                    raise ValueError(f"packet sequence gap {previous_packet}->{packet}")
                if previous_block is not None and block_sequence != ((previous_block + 1) & 0xFFFFFFFF):
                    result["sequence_gaps"].append({"expected_block": previous_block + 1, "actual_block": block_sequence})
                    raise ValueError(f"block sequence gap {previous_block}->{block_sequence}")
                previous_packet, previous_block = packet, block_sequence
                result["validated_frames"] += 1
                result["validated_bytes"] += len(header) + len(payload)
                result["last_packet"] = packet
                result["last_block"] = block_sequence
                if result["validated_frames"] == 1 or result["validated_frames"] % args.show_every == 0:
                    print(f"REAL frame={result['validated_frames']} packet={packet} block={block_sequence} "
                          f"bytes={length} records={record_count} mask=0x{channel_mask:x} "
                          f"counts={channel_counts} flags=0x{flags:x} "
                          f"CRC=OK samples={samples}", flush=True)
        result["status"] = "PASS" if result["validated_frames"] else "FAIL"
        result["stage"] = "complete"
    except KeyboardInterrupt:
        result["status"] = "STOPPED" if result["validated_frames"] else "FAIL"
        result["stage"] = "keyboard_interrupt"
    except Exception as error:  # keep the evidence JSON even on a malformed frame or unplug
        result["errors"].append(f"{type(error).__name__}: {error}")
        result["stage"] = "error"
    finally:
        if raw:
            raw_path.write_bytes(raw)
        result["elapsed_s"] = time.monotonic() - started
        json_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False), flush=True)
        print(f"Evidence: {json_path}", flush=True)
    return 0 if result["status"] in ("PASS", "STOPPED") else 1


if __name__ == "__main__":
    raise SystemExit(main())
