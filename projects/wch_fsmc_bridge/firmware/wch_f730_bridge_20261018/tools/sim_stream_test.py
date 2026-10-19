"""Verify CH32V307's internal-source UBR1 stream, including content and CRC."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import struct
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports


ROOT = Path(__file__).resolve().parents[1]
VID_PID = (0x1A86, 0xFE0C)
LENGTHS = (312, 16432, 32800)


def selected_port() -> str:
    matches = [p.device for p in list_ports.comports()
               if (p.vid, p.pid) == VID_PID]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one WCH application USB port; found {matches}")
    return matches[0]


def expected_block(sequence: int) -> bytes:
    count = (1, 66, 132)[sequence % 3]
    records = bytearray()
    for index in range(count):
        record = bytearray(248)
        struct.pack_into("<IBBHII", record, 0, 0x3146524E, 1, 0, 1,
                         (sequence * 132 + index) & 0xFFFFFFFF, 0x53494D31)
        struct.pack_into("<H", record, 36, 204)
        struct.pack_into("<H", record, 38, binascii.crc_hqx(record[:38], 0xFFFF))
        record[40:244] = bytes((sequence + index + j) & 255 for j in range(204))
        struct.pack_into("<H", record, 40, 0xA55A)
        struct.pack_into("<H", record, 44, 96)
        struct.pack_into("<I", record, 244, binascii.crc32(record[40:244]))
        records.extend(record)
    header = bytearray(64)
    struct.pack_into("<IHHIIII", header, 0, 0x31425046, 1, 64, sequence,
                     0x53494D31, len(records), 1)
    struct.pack_into("<I", header, 40, count)
    struct.pack_into("<I", header, 60, binascii.crc32(records))
    return bytes(header + records)


def exact(port: serial.Serial, length: int, timeout: float) -> bytes:
    result = bytearray()
    deadline = time.monotonic() + timeout
    while len(result) < length:
        if time.monotonic() >= deadline:
            raise TimeoutError(f"read timeout: {len(result)}/{length} bytes")
        result.extend(port.read(length - len(result)))
    return bytes(result)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="defaults to auto-detecting 1A86:FE0C")
    parser.add_argument("--frames", type=int, default=30)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--show-every", type=int, default=1,
                        help="print one validated SIM frame every N frames")
    parser.add_argument("--hex", type=Path, help="selected local HEX; not Flash readback")
    parser.add_argument("--evidence-dir", type=Path,
                        help="defaults to ignored build/results")
    args = parser.parse_args()
    if args.frames < 3 or args.timeout <= 0 or args.show_every < 1:
        parser.error("frames must be at least 3; timeout and show-every must be positive")
    if args.hex and not args.hex.is_file():
        parser.error(f"selected HEX does not exist: {args.hex}")
    evidence = args.evidence_dir or ROOT / "build" / "results"
    evidence.mkdir(parents=True, exist_ok=True)
    output = evidence / ("sim_stream_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".json")
    result = {"status": "FAIL", "stage": "start", "timestamp": datetime.now().astimezone().isoformat(),
              "parameters": {"port": args.port, "frames": args.frames, "timeout": args.timeout,
                             "hex": str(args.hex) if args.hex else None},
              "validated_frames": 0, "validated_bytes": 0}
    if args.hex and args.hex.is_file():
        result["selected_hex_sha256"] = hashlib.sha256(args.hex.read_bytes()).hexdigest()
        result["identity_scope"] = "local_file_only_not_flash_readback"
    started = time.monotonic()
    frame_bytes = b""
    try:
        result["stage"] = "open_port"
        result["port"] = args.port or selected_port()
        with serial.Serial(result["port"], 115200, timeout=0.1,
                           write_timeout=args.timeout) as port:
            port.dtr = True
            result["stage"] = "initial_seek"
            window = bytearray()
            seek_deadline = time.monotonic() + args.timeout
            while bytes(window) != b"UBR1":
                if time.monotonic() >= seek_deadline:
                    raise TimeoutError("No UBR1 frame after opening CDC port")
                window.extend(port.read(1))
                if len(window) > 4:
                    del window[:-4]
            previous = None
            for index in range(args.frames):
                result["stage"] = "frame_header"
                header = (b"UBR1" + exact(port, 20, args.timeout)) if index == 0 else exact(port, 24, args.timeout)
                frame_bytes = header
                magic, version, kind, packet_sequence, block_sequence, length, crc = struct.unpack("<IHHIIII", header)
                if (magic, version, kind) != (0x31524255, 1, 1) or length not in LENGTHS:
                    raise ValueError(f"invalid UBR1 header at frame {index + 1}")
                result["stage"] = "frame_payload"
                payload = exact(port, length, args.timeout)
                frame_bytes += payload
                result["stage"] = "validate"
                if binascii.crc32(payload) != crc:
                    raise ValueError(f"USB payload CRC mismatch at frame {index + 1}")
                if packet_sequence != block_sequence:
                    raise ValueError(f"packet/block sequence mismatch at frame {index + 1}")
                if previous is not None and packet_sequence != ((previous + 1) & 0xFFFFFFFF):
                    raise ValueError(f"missing or reordered frame after sequence {previous}")
                if payload != expected_block(block_sequence):
                    raise ValueError(f"block content mismatch at frame {index + 1}")
                previous = packet_sequence
                result["validated_frames"] += 1
                result["validated_bytes"] += len(frame_bytes)
                result["last_sequence"] = packet_sequence
                if index == 0 or (index + 1) % args.show_every == 0:
                    samples = struct.unpack_from("<12h", payload, 64 + 52)
                    print(f"SIM block={block_sequence} bytes={length} "
                          f"CRC=OK samples={samples}", flush=True)
            result["status"] = "PASS"
            result["stage"] = "complete"
        return 0
    except (OSError, RuntimeError, ValueError, TimeoutError, serial.SerialException) as error:
        result["error"] = f"{type(error).__name__}: {error}"
        if frame_bytes:
            raw = output.with_suffix(".bin")
            raw.write_bytes(frame_bytes)
            result["first_failed_frame_bytes"] = str(raw)
        return 1
    finally:
        result["elapsed_s"] = time.monotonic() - started
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False), flush=True)
        print(f"Evidence: {output}", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())

