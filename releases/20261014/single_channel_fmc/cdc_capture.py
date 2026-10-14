#!/usr/bin/env python3
"""Read and validate UBR1 data frames from the F730 USB CDC ACM port.

The MC007 firmware exposes a CDC serial device, not a WinUSB vendor interface.
This tool therefore treats USB as a byte stream and reuses the frozen UBR1,
block, and record validators shipped with the F730 package.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

try:
    import serial  # type: ignore[import-not-found]
except ImportError as exc:  # pragma: no cover - environment dependent
    raise SystemExit("pyserial is required: python -m pip install pyserial") from exc

TOOLS = Path(__file__).resolve().parent / "tools"
sys.path.insert(0, str(TOOLS))

from vendor_bulk_host import (  # noqa: E402
    ProtocolError,
    UsbStreamDecoder,
    USB_TYPE_DATA,
    _feed_all,
    run_self_test,
    validate_block,
)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate UBR1 frames received from the F730 CDC ACM port."
    )
    parser.add_argument("--self-test", action="store_true", help="run offline golden-vector checks")
    parser.add_argument("--port", help="Windows COM port, for example COM6")
    parser.add_argument("--baud", type=int, default=115200, help="CDC line coding value")
    parser.add_argument("--blocks", type=int, default=1, help="validated DATA blocks to receive")
    parser.add_argument("--overall-timeout-s", type=float, default=30.0)
    parser.add_argument("--read-size", type=int, default=4096)
    parser.add_argument("--output", type=Path, help="save complete validated UBR1 frames")
    return parser


def _live_serial(args: argparse.Namespace) -> int:
    if not args.port:
        raise SystemExit("--port is required for live mode")
    if args.blocks < 1:
        raise SystemExit("--blocks must be at least 1")

    golden = TOOLS.parent / "protocol" / "golden"
    decoder = UsbStreamDecoder()
    deadline = time.monotonic() + args.overall_timeout_s
    data_blocks = 0
    last_packet_sequence: int | None = None
    output = args.output.open("wb") if args.output else None
    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as port:
            print(f"CDC_OPEN_OK port={args.port} baud={args.baud}")
            while time.monotonic() < deadline and data_blocks < args.blocks:
                chunk = port.read(max(1, args.read_size))
                if not chunk:
                    continue
                for frame in decoder.feed(chunk):
                    if last_packet_sequence is not None:
                        expected = (last_packet_sequence + 1) & 0xFFFFFFFF
                        if frame.packet_sequence != expected:
                            raise ProtocolError(
                                f"USB packet sequence {frame.packet_sequence}, expected {expected}"
                            )
                    last_packet_sequence = frame.packet_sequence
                    if output:
                        output.write(frame.wire)
                    if frame.frame_type != USB_TYPE_DATA:
                        print(
                            f"FRAME type={frame.frame_type} message=0x{frame.message_id:08X} "
                            f"bytes={len(frame.payload)}"
                        )
                        continue
                    info = validate_block(frame.payload)
                    if info["block_sequence"] != frame.message_id:
                        raise ProtocolError("DATA message id/block sequence mismatch")
                    data_blocks += 1
                    print(
                        f"DATA_OK packet={frame.packet_sequence} block={frame.message_id} "
                        f"records={info['record_count']} bytes={len(frame.payload)}"
                    )
            if data_blocks < args.blocks:
                raise TimeoutError(
                    f"timeout: received {data_blocks}/{args.blocks} DATA blocks"
                )
            # Capture ends intentionally in a live stream; trailing partial next frame is excluded.
            if decoder.discarded_bytes:
                raise ProtocolError(f"Discarded stream bytes: {decoder.discarded_bytes}")
            print(
                f"CDC_BRIDGE_TEST_OK blocks={data_blocks} discarded={decoder.discarded_bytes}"
            )
            return 0
    finally:
        if output:
            output.close()


def main() -> int:
    args = _build_parser().parse_args()
    if args.self_test:
        if args.port:
            raise SystemExit("--self-test cannot be combined with --port")
        return run_self_test(TOOLS.parent / "protocol" / "golden")
    return _live_serial(args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ProtocolError, TimeoutError, serial.SerialException) as exc:
        raise SystemExit(f"CDC_BRIDGE_ERROR: {exc}") from exc
