"""Check the CH32V307 demo's USB CDC byte echo; this is not an FPGA test."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports


ROOT = Path(__file__).resolve().parents[1]
VID, PID = 0x1A86, 0xFE0C


def find_port() -> str:
    matches = [p.device for p in list_ports.comports() if (p.vid, p.pid) == (VID, PID)]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one test CDC device {VID:04X}:{PID:04X}; found {matches}")
    return matches[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="CDC COM port; omit to detect VID:PID 1A86:FE0C")
    parser.add_argument("--rounds", type=int, default=100)
    parser.add_argument("--seconds", type=float, default=0,
                        help="run for at least this many seconds instead of a fixed round count")
    parser.add_argument("--progress-seconds", type=float, default=60,
                        help="progress interval for duration runs")
    parser.add_argument("--bytes", type=int, default=64, dest="packet_bytes")
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--hex", type=Path, help="selected HEX file for evidence; not Flash readback")
    parser.add_argument("--evidence-dir", type=Path,
                        help="where to write result JSON; defaults to ignored build/results")
    args = parser.parse_args()
    if (args.rounds <= 0 or args.packet_bytes <= 0 or args.timeout <= 0
            or args.seconds < 0 or args.progress_seconds <= 0):
        parser.error("rounds, bytes, timeout, and progress-seconds must be positive; seconds must be nonnegative")

    evidence = args.evidence_dir or ROOT / "build" / "results"
    evidence.mkdir(parents=True, exist_ok=True)
    parameters = vars(args).copy()
    parameters["hex"] = str(args.hex) if args.hex else None
    record = {"status": "FAIL", "stage": "start", "parameters": parameters,
              "timestamp": datetime.now().astimezone().isoformat()}
    firmware = args.hex or ROOT / "release" / "WchF730Bridge.hex"
    if firmware.is_file():
        record["selected_hex_path"] = str(firmware.resolve())
        record["selected_hex_sha256"] = hashlib.sha256(firmware.read_bytes()).hexdigest()
        record["identity_scope"] = "local_file_only_not_flash_readback"
    output = evidence / ("cdc_echo_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".json")
    started = time.monotonic()
    try:
        record["stage"] = "find_port"
        port_name = args.port or find_port()
        record["port"] = port_name
        record["stage"] = "open_port"
        with serial.Serial(port_name, 115200, timeout=0.05, write_timeout=args.timeout) as port:
            record["stage"] = "echo"
            rng = random.Random(0x307)
            test_started = time.monotonic()
            next_progress = test_started + args.progress_seconds
            round_index = 0
            while ((time.monotonic() - test_started < args.seconds) if args.seconds
                   else (round_index < args.rounds)):
                packet = bytes(rng.getrandbits(8) for _ in range(args.packet_bytes))
                deadline = time.monotonic() + args.timeout
                if port.write(packet) != len(packet):
                    raise RuntimeError(f"short write at round {round_index + 1}")
                received = bytearray()
                while len(received) < len(packet) and time.monotonic() < deadline:
                    received.extend(port.read(min(64, len(packet) - len(received))))
                if bytes(received) != packet:
                    raise RuntimeError(
                        f"round {round_index + 1}: received {len(received)}/{len(packet)} bytes; "
                        f"first mismatch at {next((i for i, (a, b) in enumerate(zip(packet, received)) if a != b), 'timeout')}"
                    )
                record["last_valid_round"] = round_index + 1
                round_index += 1
                if args.seconds and time.monotonic() >= next_progress:
                    print(f"PROGRESS: rounds={round_index} bytes={round_index * args.packet_bytes} "
                          f"elapsed_s={time.monotonic() - test_started:.1f}", flush=True)
                    next_progress = time.monotonic() + args.progress_seconds
        record.update(status="PASS", stage="complete",
                      verified_bytes=round_index * args.packet_bytes,
                      run_elapsed_s=time.monotonic() - test_started)
        return 0
    except Exception as error:
        record["error"] = f"{type(error).__name__}: {error}"
        return 1
    finally:
        record["elapsed_s"] = time.monotonic() - started
        output.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(record, ensure_ascii=False))
        print(f"Evidence: {output}")


if __name__ == "__main__":
    raise SystemExit(main())

