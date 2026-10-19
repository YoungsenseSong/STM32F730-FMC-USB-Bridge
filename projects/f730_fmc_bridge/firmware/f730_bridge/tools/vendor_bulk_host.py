#!/usr/bin/env python3
"""Offline codec check and live USB Vendor Bulk acceptance tool.

The offline self-test has no third-party dependency.  Live access requires
PyUSB and a libusb/WinUSB backend.  This tool deliberately requires an
explicit VID/PID; the firmware does not contain an unassigned test identity.
"""

from __future__ import annotations

import argparse
import binascii
import json
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


RECORD_MAGIC = 0x3146524E
RECORD_VERSION = 1
RECORD_BYTES = 248
RF_FRAME_BYTES = 204
RF_FRAME_MAGIC = 0xA55A
RF_MAX_SAMPLES = 96

BLOCK_MAGIC = 0x31425046
BLOCK_VERSION = 1
BLOCK_HEADER_BYTES = 64
BLOCK_MAX_RECORDS = 132
BLOCK_MAX_PAYLOAD_BYTES = BLOCK_MAX_RECORDS * RECORD_BYTES
BLOCK_MAX_BYTES = BLOCK_HEADER_BYTES + BLOCK_MAX_PAYLOAD_BYTES

USB_MAGIC_BYTES = b"UBR1"
USB_MAGIC = 0x31524255
USB_VERSION = 1
USB_HEADER_BYTES = 24
USB_TYPE_DATA = 1
USB_TYPE_CMD = 2
USB_TYPE_RSP = 3
USB_TYPE_EVENT = 4

VENDOR_INTERFACE_CLASS = 0xFF
VENDOR_OUT_EP = 0x01
VENDOR_IN_EP = 0x81
USB_FS_MAX_PACKET = 64


class ProtocolError(ValueError):
    """A received byte stream violates the frozen bridge contract."""


@dataclass(frozen=True)
class UsbFrame:
    frame_type: int
    packet_sequence: int
    message_id: int
    payload: bytes
    wire: bytes


def _crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def _crc16_ccitt_false(data: bytes) -> int:
    return binascii.crc_hqx(data, 0xFFFF)


def validate_record(record: bytes) -> dict[str, int]:
    if len(record) != RECORD_BYTES:
        raise ProtocolError(f"record length {len(record)} != {RECORD_BYTES}")
    magic, version, node_id, record_type = struct.unpack_from("<IBBH", record, 0)
    if magic != RECORD_MAGIC:
        raise ProtocolError(f"record magic 0x{magic:08X}")
    if version != RECORD_VERSION:
        raise ProtocolError(f"record version {version}")
    if record_type != 1:
        raise ProtocolError(f"record type {record_type}")
    payload_bytes = struct.unpack_from("<H", record, 36)[0]
    if payload_bytes != RF_FRAME_BYTES:
        raise ProtocolError(f"record payload length {payload_bytes}")
    expected_header_crc = struct.unpack_from("<H", record, 38)[0]
    if _crc16_ccitt_false(record[:38]) != expected_header_crc:
        raise ProtocolError("record header CRC16")
    expected_payload_crc = struct.unpack_from("<I", record, 244)[0]
    if _crc32(record[40:244]) != expected_payload_crc:
        raise ProtocolError("record payload CRC32")
    rf_magic, _, sample_count = struct.unpack_from("<HHH", record, 40)
    if rf_magic != RF_FRAME_MAGIC:
        raise ProtocolError(f"RF frame magic 0x{rf_magic:04X}")
    if not 1 <= sample_count <= RF_MAX_SAMPLES:
        raise ProtocolError(f"RF sample count {sample_count}")
    return {
        "node_id": node_id,
        "transport_sequence": struct.unpack_from("<I", record, 8)[0],
        "sync_epoch": struct.unpack_from("<I", record, 12)[0],
        "sample_count": sample_count,
    }


def validate_block(block: bytes) -> dict[str, object]:
    if len(block) < BLOCK_HEADER_BYTES:
        raise ProtocolError(f"block length {len(block)} < {BLOCK_HEADER_BYTES}")
    fields = struct.unpack_from("<IHHIIIIQQIIIIII", block, 0)
    (
        magic,
        version,
        header_bytes,
        block_sequence,
        sync_epoch,
        payload_bytes,
        channel_mask,
        first_tick,
        last_tick,
        count0,
        count1,
        count2,
        count3,
        flags,
        payload_crc,
    ) = fields
    counts = (count0, count1, count2, count3)
    if magic != BLOCK_MAGIC:
        raise ProtocolError(f"block magic 0x{magic:08X}")
    if version != BLOCK_VERSION:
        raise ProtocolError(f"block version {version}")
    if header_bytes != BLOCK_HEADER_BYTES:
        raise ProtocolError(f"block header length {header_bytes}")
    if not 0 < payload_bytes <= BLOCK_MAX_PAYLOAD_BYTES:
        raise ProtocolError(f"block payload length {payload_bytes}")
    if payload_bytes % RECORD_BYTES:
        raise ProtocolError("block payload splits a record")
    if len(block) != BLOCK_HEADER_BYTES + payload_bytes:
        raise ProtocolError(
            f"block wire length {len(block)} != {BLOCK_HEADER_BYTES + payload_bytes}"
        )
    count_mask = sum((1 << channel) for channel, count in enumerate(counts) if count)
    if channel_mask & ~0x0F or channel_mask != count_mask:
        raise ProtocolError(
            f"block channel mask 0x{channel_mask:08X} != count mask 0x{count_mask:02X}"
        )
    if sum(counts) != payload_bytes // RECORD_BYTES:
        raise ProtocolError("block record counts do not match payload length")
    if last_tick < first_tick:
        raise ProtocolError("block FPGA tick range is reversed")
    if flags & 0x0F:
        raise ProtocolError(f"block error flags 0x{flags:08X}")
    payload = block[BLOCK_HEADER_BYTES:]
    if _crc32(payload) != payload_crc:
        raise ProtocolError("block payload CRC32")

    records = []
    for offset in range(0, payload_bytes, RECORD_BYTES):
        records.append(validate_record(payload[offset : offset + RECORD_BYTES]))
    return {
        "block_sequence": block_sequence,
        "sync_epoch": sync_epoch,
        "payload_bytes": payload_bytes,
        "record_count": len(records),
        "channel_counts": counts,
        "records": records,
    }


def build_usb_frame(
    frame_type: int,
    packet_sequence: int,
    message_id: int,
    payload: bytes,
) -> bytes:
    if frame_type not in (USB_TYPE_DATA, USB_TYPE_CMD, USB_TYPE_RSP, USB_TYPE_EVENT):
        raise ProtocolError(f"USB frame type {frame_type}")
    if len(payload) > BLOCK_MAX_BYTES:
        raise ProtocolError(f"USB payload length {len(payload)}")
    return struct.pack(
        "<IHHIIII",
        USB_MAGIC,
        USB_VERSION,
        frame_type,
        packet_sequence & 0xFFFFFFFF,
        message_id & 0xFFFFFFFF,
        len(payload),
        _crc32(payload),
    ) + payload


class UsbStreamDecoder:
    def __init__(self) -> None:
        self._buffer = bytearray()
        self.discarded_bytes = 0

    def feed(self, chunk: bytes) -> list[UsbFrame]:
        self._buffer.extend(chunk)
        frames: list[UsbFrame] = []
        while True:
            if len(self._buffer) < 4:
                break
            magic_offset = self._buffer.find(USB_MAGIC_BYTES)
            if magic_offset < 0:
                keep = min(3, len(self._buffer))
                self.discarded_bytes += len(self._buffer) - keep
                del self._buffer[:-keep]
                break
            if magic_offset:
                self.discarded_bytes += magic_offset
                del self._buffer[:magic_offset]
            if len(self._buffer) < USB_HEADER_BYTES:
                break

            magic, version, frame_type, sequence, message_id, length, crc = (
                struct.unpack_from("<IHHIIII", self._buffer, 0)
            )
            if magic != USB_MAGIC:
                raise AssertionError("magic search and decode disagree")
            if version != USB_VERSION:
                raise ProtocolError(f"USB protocol version {version}")
            if frame_type not in (
                USB_TYPE_DATA,
                USB_TYPE_CMD,
                USB_TYPE_RSP,
                USB_TYPE_EVENT,
            ):
                raise ProtocolError(f"USB frame type {frame_type}")
            if length > BLOCK_MAX_BYTES:
                raise ProtocolError(f"USB payload length {length}")
            wire_bytes = USB_HEADER_BYTES + length
            if len(self._buffer) < wire_bytes:
                break
            wire = bytes(self._buffer[:wire_bytes])
            payload = wire[USB_HEADER_BYTES:]
            if _crc32(payload) != crc:
                raise ProtocolError(
                    f"USB payload CRC32 for packet sequence {sequence}"
                )
            frames.append(UsbFrame(frame_type, sequence, message_id, payload, wire))
            del self._buffer[:wire_bytes]
        return frames

    def finish(self) -> None:
        if self._buffer:
            raise ProtocolError(f"truncated USB stream ({len(self._buffer)} bytes remain)")


def _feed_all(decoder: UsbStreamDecoder, chunks: Iterable[bytes]) -> list[UsbFrame]:
    frames: list[UsbFrame] = []
    for chunk in chunks:
        frames.extend(decoder.feed(chunk))
    return frames


def run_self_test(golden_dir: Path) -> int:
    fragments = json.loads((golden_dir / "usb_stream_fragments.json").read_text())
    chunks = [bytes.fromhex(value) for value in fragments["chunks_hex"]]
    decoder = UsbStreamDecoder()
    frames = _feed_all(decoder, chunks)
    decoder.finish()
    if decoder.discarded_bytes or len(frames) != 2:
        raise ProtocolError("golden USB fragment reconstruction")
    if frames[0].frame_type != USB_TYPE_DATA or frames[1].frame_type != USB_TYPE_CMD:
        raise ProtocolError("golden USB frame ordering")
    block_info = validate_block(frames[0].payload)
    if block_info["block_sequence"] != frames[0].message_id:
        raise ProtocolError("DATA message id does not match block sequence")

    valid_record = (golden_dir / "nrf_record_valid.bin").read_bytes()
    validate_record(valid_record)
    bad_record = bytearray(valid_record)
    bad_record[100] ^= 0x80
    try:
        validate_record(bytes(bad_record))
    except ProtocolError:
        pass
    else:
        raise ProtocolError("damaged record was accepted")

    bad_frame = bytearray(frames[0].wire)
    bad_frame[-1] ^= 0x01
    try:
        UsbStreamDecoder().feed(bytes(bad_frame))
    except ProtocolError:
        pass
    else:
        raise ProtocolError("damaged USB frame was accepted")

    print(
        "VENDOR_HOST_SELF_TEST_OK "
        f"frames={len(frames)} block_records={block_info['record_count']}"
    )
    return 0


def _parse_u16(value: str) -> int:
    parsed = int(value, 0)
    if not 0 < parsed <= 0xFFFF:
        raise argparse.ArgumentTypeError("value must be in 0x0001..0xFFFF")
    return parsed


def _parse_u32(value: str) -> int:
    parsed = int(value, 0)
    if not 0 <= parsed <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("value must be in 0..0xFFFFFFFF")
    return parsed


def _live_usb(args: argparse.Namespace) -> int:
    try:
        import usb.core  # type: ignore[import-not-found]
        import usb.util  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError(
            "live mode requires PyUSB and a libusb backend; on Windows bind "
            "interface 0 to WinUSB before testing"
        ) from error

    try:
        device = usb.core.find(idVendor=args.vid, idProduct=args.pid)
    except usb.core.USBError as error:
        raise RuntimeError(f"USB backend error while searching: {error}") from error
    if device is None:
        raise RuntimeError(f"USB device {args.vid:04X}:{args.pid:04X} not found")

    interface_number = None
    try:
        device.set_configuration()
        configuration = device.get_active_configuration()
        interface = None
        for candidate in configuration:
            if candidate.bInterfaceClass == VENDOR_INTERFACE_CLASS:
                interface = candidate
                break
        if interface is None:
            raise RuntimeError("no vendor-specific interface (class 0xFF) found")
        interface_number = interface.bInterfaceNumber
        usb.util.claim_interface(device, interface_number)

        endpoints = {endpoint.bEndpointAddress: endpoint for endpoint in interface}
        if VENDOR_IN_EP not in endpoints or VENDOR_OUT_EP not in endpoints:
            raise RuntimeError(
                f"expected endpoints OUT 0x{VENDOR_OUT_EP:02X}/IN 0x{VENDOR_IN_EP:02X}"
            )
        for address in (VENDOR_OUT_EP, VENDOR_IN_EP):
            endpoint = endpoints[address]
            if usb.util.endpoint_type(endpoint.bmAttributes) != usb.util.ENDPOINT_TYPE_BULK:
                raise RuntimeError(f"endpoint 0x{address:02X} is not Bulk")
            if endpoint.wMaxPacketSize != USB_FS_MAX_PACKET:
                raise RuntimeError(
                    f"endpoint 0x{address:02X} max packet {endpoint.wMaxPacketSize}, expected 64"
                )

        print(
            f"ENUM_OK vid=0x{args.vid:04X} pid=0x{args.pid:04X} "
            f"interface={interface_number} out=0x{VENDOR_OUT_EP:02X} "
            f"in=0x{VENDOR_IN_EP:02X} mps={USB_FS_MAX_PACKET}"
        )

        if args.blocks == 0 and args.probe_unsupported is None:
            return 0

        if args.probe_unsupported is not None:
            command = build_usb_frame(
                USB_TYPE_CMD,
                0,
                args.probe_unsupported,
                b"",
            )
            written = device.write(VENDOR_OUT_EP, command, timeout=args.timeout_ms)
            if written != len(command):
                raise RuntimeError(f"short USB OUT write {written}/{len(command)}")

        deadline = time.monotonic() + args.overall_timeout_s
        decoder = UsbStreamDecoder()
        data_blocks = 0
        response_seen = args.probe_unsupported is None
        last_packet_sequence: int | None = None
        output = args.output.open("wb") if args.output is not None else None
        try:
            while time.monotonic() < deadline:
                try:
                    chunk = bytes(
                        device.read(
                            VENDOR_IN_EP,
                            USB_FS_MAX_PACKET,
                            timeout=args.timeout_ms,
                        )
                    )
                except usb.core.USBTimeoutError:
                    continue
                for frame in decoder.feed(chunk):
                    if output is not None:
                        output.write(frame.wire)
                    if last_packet_sequence is not None:
                        expected = (last_packet_sequence + 1) & 0xFFFFFFFF
                        if frame.packet_sequence != expected:
                            raise ProtocolError(
                                f"USB packet sequence {frame.packet_sequence}, expected {expected}"
                            )
                    last_packet_sequence = frame.packet_sequence

                    if frame.frame_type == USB_TYPE_DATA:
                        info = validate_block(frame.payload)
                        if info["block_sequence"] != frame.message_id:
                            raise ProtocolError("DATA message id/block sequence mismatch")
                        data_blocks += 1
                        print(
                            "DATA_OK "
                            f"packet={frame.packet_sequence} block={frame.message_id} "
                            f"records={info['record_count']} bytes={len(frame.payload)}"
                        )
                    elif frame.frame_type == USB_TYPE_RSP:
                        if frame.message_id == args.probe_unsupported:
                            if len(frame.payload) != 4:
                                raise ProtocolError("unsupported-command RSP length")
                            status = struct.unpack("<i", frame.payload)[0]
                            if status != -5:
                                raise ProtocolError(
                                    f"unsupported-command status {status}, expected -5"
                                )
                            response_seen = True
                            print(
                                f"RSP_OK command=0x{frame.message_id:08X} status={status}"
                            )
                        else:
                            print(
                                f"RSP_OTHER command=0x{frame.message_id:08X} "
                                f"bytes={len(frame.payload)}"
                            )
                    else:
                        print(
                            f"FRAME type={frame.frame_type} message=0x{frame.message_id:08X} "
                            f"bytes={len(frame.payload)}"
                        )

                if data_blocks >= args.blocks and response_seen:
                    print(
                        "VENDOR_BULK_TEST_OK "
                        f"blocks={data_blocks} discarded={decoder.discarded_bytes}"
                    )
                    return 0
        finally:
            if output is not None:
                output.close()

        raise RuntimeError(
            f"overall timeout: blocks={data_blocks}/{args.blocks}, "
            f"response_seen={response_seen}"
        )
    except usb.core.USBError as error:
        raise RuntimeError(f"USB transfer/configuration error: {error}") from error
    finally:
        if interface_number is not None:
            try:
                usb.util.release_interface(device, interface_number)
            except Exception:
                pass
        usb.util.dispose_resources(device)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate the F730 Vendor Bulk bridge offline or on hardware."
    )
    parser.add_argument("--self-test", action="store_true", help="run without USB hardware")
    parser.add_argument(
        "--golden-dir",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "protocol" / "golden",
    )
    parser.add_argument("--vid", type=_parse_u16, help="authorized USB VID")
    parser.add_argument("--pid", type=_parse_u16, help="authorized USB PID")
    parser.add_argument("--blocks", type=int, default=1, help="DATA blocks to validate")
    parser.add_argument("--timeout-ms", type=int, default=1000)
    parser.add_argument("--overall-timeout-s", type=float, default=30.0)
    parser.add_argument(
        "--probe-unsupported",
        type=_parse_u32,
        help="send this unknown command id and require signed status -5",
    )
    parser.add_argument("--output", type=Path, help="save complete validated USB frames")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    if args.blocks < 0:
        parser.error("--blocks must be non-negative")
    if args.timeout_ms <= 0 or args.overall_timeout_s <= 0:
        parser.error("timeouts must be positive")

    try:
        if args.self_test:
            if args.vid is not None or args.pid is not None:
                parser.error("--self-test cannot be combined with --vid/--pid")
            return run_self_test(args.golden_dir)
        if args.vid is None or args.pid is None:
            parser.error("live mode requires both --vid and --pid")
        return _live_usb(args)
    except (OSError, ProtocolError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
