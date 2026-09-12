"""Generate deterministic cross-device bridge protocol golden vectors."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import struct
from pathlib import Path


FPGA_RECORD_MAGIC = 0x3146524E
FPGA_RECORD_VERSION = 1
FPGA_RECORD_BYTES = 248
RF_FRAME_MAGIC = 0xA55A
RF_FRAME_BYTES = 204
RF_SAMPLE_COUNT = 96

BLOCK_MAGIC = 0x31425046
BLOCK_VERSION = 1
BLOCK_HEADER_BYTES = 64

USB_MAGIC = 0x31524255
USB_VERSION = 1
USB_TYPE_DATA = 1
USB_TYPE_CMD = 2
USB_HEADER_BYTES = 24


def crc16_ccitt_false(data: bytes) -> int:
    return binascii.crc_hqx(data, 0xFFFF)


def crc32_ieee(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def build_rf_frame(rf_sequence: int, timestamp_ms: int, sample_bias: int) -> bytes:
    samples = tuple((index * 257 + sample_bias) for index in range(RF_SAMPLE_COUNT))
    if not all(-32768 <= sample <= 32767 for sample in samples):
        raise ValueError("sample formula exceeded int16 range")
    frame = struct.pack(
        "<HHHHI96h",
        RF_FRAME_MAGIC,
        rf_sequence,
        RF_SAMPLE_COUNT,
        0x0006,  # MEMS | BATCH_START in the V1 RF frame.
        timestamp_ms,
        *samples,
    )
    assert len(frame) == RF_FRAME_BYTES
    return frame


def build_record(
    *,
    node_id: int,
    transport_seq: int,
    sync_epoch: int,
    rx_tick: int,
    logical_sample_index: int,
    rf_sequence: int,
    timestamp_ms: int,
    sample_bias: int,
) -> bytes:
    frame = build_rf_frame(rf_sequence, timestamp_ms, sample_bias)
    header_without_crc = struct.pack(
        "<IBBHIIQQIH",
        FPGA_RECORD_MAGIC,
        FPGA_RECORD_VERSION,
        node_id,
        1,
        transport_seq,
        sync_epoch,
        rx_tick,
        logical_sample_index,
        0x00000029,  # VALID | BATCH_START | SYNCED.
        len(frame),
    )
    assert len(header_without_crc) == 38
    record = (
        header_without_crc
        + struct.pack("<H", crc16_ccitt_false(header_without_crc))
        + frame
        + struct.pack("<I", crc32_ieee(frame))
    )
    assert len(record) == FPGA_RECORD_BYTES
    return record


def build_block(records: list[bytes]) -> bytes:
    if not records or any(len(record) != FPGA_RECORD_BYTES for record in records):
        raise ValueError("block payload must contain complete 248-byte records")
    payload = b"".join(records)
    header = struct.pack(
        "<IHHIIIIQQIIIIII",
        BLOCK_MAGIC,
        BLOCK_VERSION,
        BLOCK_HEADER_BYTES,
        0x89ABCDEF,
        0x10203040,
        len(payload),
        0x0000000C,
        0x0102030405060708,
        0x2122232425262728,
        0,
        0,
        1,
        1,
        0,
        crc32_ieee(payload),
    )
    assert len(header) == BLOCK_HEADER_BYTES
    return header + payload


def build_usb_frame(frame_type: int, sequence: int, message_id: int, payload: bytes) -> bytes:
    header = struct.pack(
        "<IHHIIII",
        USB_MAGIC,
        USB_VERSION,
        frame_type,
        sequence,
        message_id,
        len(payload),
        crc32_ieee(payload),
    )
    assert len(header) == USB_HEADER_BYTES
    return header + payload


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def generate(output_dir: Path) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)

    record0 = build_record(
        node_id=2,
        transport_seq=0xFFFFFFFE,
        sync_epoch=0x10203040,
        rx_tick=0x0102030405060708,
        logical_sample_index=0x1112131415161718,
        rf_sequence=0xBEEF,
        timestamp_ms=0x12345678,
        sample_bias=-12345,
    )
    record1 = build_record(
        node_id=3,
        transport_seq=0xFFFFFFFF,
        sync_epoch=0x10203040,
        rx_tick=0x2122232425262728,
        logical_sample_index=0x3132333435363738,
        rf_sequence=0xBEF0,
        timestamp_ms=0x12345679,
        sample_bias=-12000,
    )

    bad_header = bytearray(record0)
    bad_header[5] ^= 0x01
    bad_payload = bytearray(record0)
    bad_payload[100] ^= 0x80

    block = build_block([record0, record1])
    usb_data = build_usb_frame(USB_TYPE_DATA, 7, 0x89ABCDEF, block)
    command_payload = struct.pack("<I", 0x10203040)
    usb_command = build_usb_frame(USB_TYPE_CMD, 8, 0x00000008, command_payload)
    usb_stream = usb_data + usb_command
    chunk_sizes = [1, 7, 56, 3, 64, 127, 5, 511]
    chunks: list[bytes] = []
    offset = 0
    chunk_index = 0
    while offset < len(usb_stream):
        size = chunk_sizes[chunk_index % len(chunk_sizes)]
        chunks.append(usb_stream[offset : offset + size])
        offset += size
        chunk_index += 1

    binary_vectors = {
        "nrf_record_valid.bin": record0,
        "nrf_record_header_crc_bad.bin": bytes(bad_header),
        "nrf_record_payload_crc_bad.bin": bytes(bad_payload),
        "fpga_block_two_records.bin": block,
        "usb_data_frame.bin": usb_data,
        "usb_stream_two_frames.bin": usb_stream,
    }
    for filename, data in binary_vectors.items():
        (output_dir / filename).write_bytes(data)

    fragments = {
        "source": "usb_stream_two_frames.bin",
        "chunk_sizes": [len(chunk) for chunk in chunks],
        "chunks_hex": [chunk.hex() for chunk in chunks],
    }
    fragments_data = (json.dumps(fragments, indent=2) + "\n").encode()
    (output_dir / "usb_stream_fragments.json").write_bytes(fragments_data)

    all_outputs = {**binary_vectors, "usb_stream_fragments.json": fragments_data}
    manifest: dict[str, object] = {
        "contract_version": 1,
        "record": {
            "bytes": FPGA_RECORD_BYTES,
            "header_crc16": struct.unpack_from("<H", record0, 38)[0],
            "payload_crc32": struct.unpack_from("<I", record0, 244)[0],
            "transport_seq": 0xFFFFFFFE,
        },
        "block": {
            "bytes": len(block),
            "payload_bytes": len(block) - BLOCK_HEADER_BYTES,
            "sequence": 0x89ABCDEF,
        },
        "usb": {
            "data_frame_bytes": len(usb_data),
            "stream_frame_count": 2,
            "fragment_count": len(chunks),
        },
        "files": {
            filename: {"bytes": len(data), "sha256": sha256(data)}
            for filename, data in sorted(all_outputs.items())
        },
    }
    manifest_data = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
    (output_dir / "manifest.json").write_bytes(manifest_data)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent / "golden",
    )
    args = parser.parse_args()
    manifest = generate(args.output)
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
