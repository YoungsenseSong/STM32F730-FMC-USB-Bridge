"""Host-side validation for the shared bridge golden vectors."""

from __future__ import annotations

import binascii
import json
import re
import struct
import tempfile
import unittest
from pathlib import Path

from . import generate_golden_vectors as vectors


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
GOLDEN = ROOT / "golden"


def _numeric_define(text: str, prefix: str, name: str) -> int:
    match = re.search(
        rf"^\s*{re.escape(prefix)}\s+{re.escape(name)}\s+([^\s/]+)",
        text,
        re.MULTILINE,
    )
    if match is None:
        raise AssertionError(f"missing {prefix} {name}")
    token = match.group(1).strip("()")
    verilog = re.fullmatch(r"\d+'([hHdD])([0-9a-fA-F_]+)", token)
    if verilog is not None:
        return int(verilog.group(2).replace("_", ""), 16 if verilog.group(1).lower() == "h" else 10)
    return int(re.sub(r"[uUlL]+$", "", token), 0)


def record_valid(record: bytes) -> bool:
    if len(record) != vectors.FPGA_RECORD_BYTES:
        return False
    magic, version, record_type, payload_len = (
        struct.unpack_from("<I", record, 0)[0],
        record[4],
        struct.unpack_from("<H", record, 6)[0],
        struct.unpack_from("<H", record, 36)[0],
    )
    return (
        magic == vectors.FPGA_RECORD_MAGIC
        and version == vectors.FPGA_RECORD_VERSION
        and record_type == 1
        and payload_len == vectors.RF_FRAME_BYTES
        and vectors.crc16_ccitt_false(record[:38]) == struct.unpack_from("<H", record, 38)[0]
        and vectors.crc32_ieee(record[40:244]) == struct.unpack_from("<I", record, 244)[0]
    )


def parse_usb_stream(chunks: list[bytes]) -> list[tuple[int, int, bytes]]:
    buffer = bytearray()
    frames: list[tuple[int, int, bytes]] = []
    for chunk in chunks:
        buffer.extend(chunk)
        while len(buffer) >= vectors.USB_HEADER_BYTES:
            magic, version, frame_type, _, message_id, payload_len, crc = struct.unpack_from(
                "<IHHIIII", buffer, 0
            )
            if magic != vectors.USB_MAGIC or version != vectors.USB_VERSION:
                raise ValueError("bad USB frame header")
            frame_len = vectors.USB_HEADER_BYTES + payload_len
            if len(buffer) < frame_len:
                break
            payload = bytes(buffer[vectors.USB_HEADER_BYTES:frame_len])
            if vectors.crc32_ieee(payload) != crc:
                raise ValueError("bad USB payload CRC")
            frames.append((frame_type, message_id, payload))
            del buffer[:frame_len]
    if buffer:
        raise ValueError("truncated USB stream")
    return frames


class ContractTests(unittest.TestCase):
    def test_01_standard_crc_checks(self) -> None:
        self.assertEqual(vectors.crc16_ccitt_false(b"123456789"), 0x29B1)
        self.assertEqual(vectors.crc32_ieee(b"123456789"), 0xCBF43926)

    def test_02_record_and_negative_vectors(self) -> None:
        self.assertTrue(record_valid((GOLDEN / "nrf_record_valid.bin").read_bytes()))
        self.assertFalse(record_valid((GOLDEN / "nrf_record_header_crc_bad.bin").read_bytes()))
        self.assertFalse(record_valid((GOLDEN / "nrf_record_payload_crc_bad.bin").read_bytes()))

    def test_03_record_layout_matches_nrf_contract(self) -> None:
        record = (GOLDEN / "nrf_record_valid.bin").read_bytes()
        self.assertEqual(record[:4], b"NRF1")
        self.assertEqual(record[4], 1)
        self.assertEqual(record[5], 2)
        self.assertEqual(struct.unpack_from("<I", record, 8)[0], 0xFFFFFFFE)
        self.assertEqual(struct.unpack_from("<H", record, 36)[0], 204)
        self.assertEqual(struct.unpack_from("<H", record, 40)[0], 0xA55A)
        self.assertEqual(len(record), 248)

    def test_04_block_contains_only_complete_records(self) -> None:
        block = (GOLDEN / "fpga_block_two_records.bin").read_bytes()
        fields = struct.unpack_from("<IHHIIII", block, 0)
        self.assertEqual(fields[:3], (vectors.BLOCK_MAGIC, 1, 64))
        self.assertEqual(fields[5], 2 * vectors.FPGA_RECORD_BYTES)
        self.assertEqual(fields[5] % vectors.FPGA_RECORD_BYTES, 0)
        payload = block[64:]
        self.assertEqual(vectors.crc32_ieee(payload), struct.unpack_from("<I", block, 60)[0])
        self.assertTrue(record_valid(payload[:248]))
        self.assertTrue(record_valid(payload[248:]))

    def test_05_usb_fragmentation_and_coalescing(self) -> None:
        description = json.loads((GOLDEN / "usb_stream_fragments.json").read_text())
        chunks = [bytes.fromhex(value) for value in description["chunks_hex"]]
        self.assertEqual(b"".join(chunks), (GOLDEN / description["source"]).read_bytes())
        frames = parse_usb_stream(chunks)
        self.assertEqual([(item[0], item[1]) for item in frames], [(1, 0x89ABCDEF), (2, 8)])
        self.assertEqual(frames[0][2], (GOLDEN / "fpga_block_two_records.bin").read_bytes())

    def test_06_checked_in_vectors_are_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            generated = Path(temp_dir)
            vectors.generate(generated)
            expected_names = sorted(path.name for path in GOLDEN.iterdir())
            actual_names = sorted(path.name for path in generated.iterdir())
            self.assertEqual(actual_names, expected_names)
            for name in expected_names:
                self.assertEqual((generated / name).read_bytes(), (GOLDEN / name).read_bytes(), name)

    def test_07_manifest_hashes(self) -> None:
        manifest = json.loads((GOLDEN / "manifest.json").read_text())
        for name, metadata in manifest["files"].items():
            data = (GOLDEN / name).read_bytes()
            self.assertEqual(len(data), metadata["bytes"])
            self.assertEqual(binascii.hexlify(__import__("hashlib").sha256(data).digest()).decode(), metadata["sha256"])

    def test_08_f730_and_zynq_constants_match_contract(self) -> None:
        c_header = (REPO_ROOT / "f730_bridge/Bridge/Inc/bridge_protocol.h").read_text()
        verilog = (
            REPO_ROOT
            / "zynq7020/zynq7020_bridge/rtl/common/protocol_defs.vh"
        ).read_text()

        c_expected = {
            "BRIDGE_RECORD_MAGIC": vectors.FPGA_RECORD_MAGIC,
            "BRIDGE_RECORD_VERSION": vectors.FPGA_RECORD_VERSION,
            "BRIDGE_RECORD_BYTES": vectors.FPGA_RECORD_BYTES,
            "BRIDGE_RF_FRAME_BYTES": vectors.RF_FRAME_BYTES,
            "BRIDGE_BLOCK_MAGIC": vectors.BLOCK_MAGIC,
            "BRIDGE_BLOCK_VERSION": vectors.BLOCK_VERSION,
            "BRIDGE_BLOCK_HEADER_BYTES": vectors.BLOCK_HEADER_BYTES,
            "BRIDGE_USB_MAGIC": vectors.USB_MAGIC,
            "BRIDGE_USB_VERSION": vectors.USB_VERSION,
            "BRIDGE_USB_HEADER_BYTES": vectors.USB_HEADER_BYTES,
        }
        for name, expected in c_expected.items():
            self.assertEqual(_numeric_define(c_header, "#define", name), expected, name)

        verilog_expected = {
            "PROTO_VERSION_VALUE": vectors.BLOCK_VERSION,
            "SPI_RECORD_BYTES": vectors.FPGA_RECORD_BYTES,
            "SPI_MAGIC_VALUE": vectors.FPGA_RECORD_MAGIC,
            "SPI_RECORD_VERSION": vectors.FPGA_RECORD_VERSION,
            "SPI_RECORD_PAYLOAD_BYTES": vectors.RF_FRAME_BYTES,
            "CRC16_POLYNOMIAL": 0x1021,
            "CRC16_INITIAL": 0xFFFF,
            "CRC32_POLYNOMIAL": 0xEDB88320,
            "CRC32_INITIAL": 0xFFFFFFFF,
            "CRC32_FINAL_XOR": 0xFFFFFFFF,
            "BLOCK_MAGIC_VALUE": vectors.BLOCK_MAGIC,
            "BLOCK_HEADER_BYTES": vectors.BLOCK_HEADER_BYTES,
            "BLOCK_TARGET_PAYLOAD_BYTES": 66 * vectors.FPGA_RECORD_BYTES,
            "BLOCK_MAX_PAYLOAD_BYTES": 132 * vectors.FPGA_RECORD_BYTES,
            "BLOCK_ACK_POLICY": 1,
        }
        for name, expected in verilog_expected.items():
            self.assertEqual(_numeric_define(verilog, "`define", name), expected, name)

        for name in (
            "CONFIG_REQUIRED_PROTO_VERSION",
            "CONFIG_REQUIRED_SPI_RECORD",
            "CONFIG_REQUIRED_SPI_MAGIC",
            "CONFIG_REQUIRED_CRC16",
            "CONFIG_REQUIRED_CRC32",
            "CONFIG_REQUIRED_CSR_BITS",
            "CONFIG_REQUIRED_BLOCK_ACK",
        ):
            self.assertEqual(_numeric_define(verilog, "`define", name), 0, name)
        self.assertEqual(_numeric_define(verilog, "`define", "CONFIG_REQUIRED_FPGA_ID"), 1)
        self.assertEqual(_numeric_define(verilog, "`define", "CONFIG_REQUIRED_SPI_COMMAND"), 1)

    def test_09_product_safety_gates_remain_explicit(self) -> None:
        identity = (
            REPO_ROOT / "f730_bridge/USB_DEVICE/App/bridge_usb_identity.h"
        ).read_text()
        self.assertEqual(
            _numeric_define(identity, "#define", "BRIDGE_USB_IDENTITY_CONFIGURED"),
            1,
        )
        self.assertEqual(
            _numeric_define(identity, "#define", "BRIDGE_USB_IDENTITY_TEST_ONLY"),
            1,
        )
        self.assertEqual(_numeric_define(identity, "#define", "BRIDGE_USB_VID"), 0x0483)
        self.assertEqual(_numeric_define(identity, "#define", "BRIDGE_USB_PID"), 0x5744)
        self.assertIn("not an allocation to this project", identity)
        self.assertIn("must remain explicitly marked test-only", identity)

        fmc_source = (REPO_ROOT / "f730_bridge/Core/Src/fmc.c").read_text()
        ioc = (REPO_ROOT / "f730_bridge/Hal_template.ioc").read_text()
        self.assertIn("FMC_WAIT_SIGNAL_ENABLE", fmc_source)
        self.assertIn("FMC_ASYNCHRONOUS_WAIT_ENABLE", fmc_source)
        self.assertIn("FMC.WaitSignal1=FMC_WAIT_SIGNAL_ENABLE", ioc)


if __name__ == "__main__":
    unittest.main(verbosity=2)
