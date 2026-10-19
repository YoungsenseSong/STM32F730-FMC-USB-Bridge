"""Live TX1 -> RX1/RX2 -> FPGA -> WCH USB terminal view."""

from __future__ import annotations

import argparse
import datetime
import json
import sys
import time
from pathlib import Path


DUAL_TOOLS = Path(__file__).resolve().parents[4] / "projects" / "03_dual_channel" / "tools"
DUAL_DEMO = DUAL_TOOLS / "dual_rx_demo"
sys.path.insert(0, str(DUAL_TOOLS))
sys.path.insert(0, str(DUAL_DEMO))

import serial  # noqa: E402
from stream_state import StreamState  # noqa: E402
from vendor_bulk_host import UsbStreamDecoder  # noqa: E402


def show(state: dict, final: bool = False) -> None:
    print(f"\n{state['elapsed_s']:7.1f}s BLOCK={state['last_block']} "
          f"MATCH={state['matched_frames']} MISMATCH={state['payload_mismatches']}", flush=True)
    for lane in state["lanes"]:
        values = ",".join(str(value) for value in lane["waveform"][-12:])
        stamp = lane["sensor_timestamp_ms"]
        sensor_time = f"{stamp / 1000:.3f}s" if stamp is not None else "--"
        status = "LIVE" if lane["live"] else "WAIT"
        print(f"  CH{lane['node_id']} RX{lane['node_id'] + 1} {status:4} "
              f"records={lane['records']:7} samples={lane['samples']:9} "
              f"RF={str(lane['last_radio']):>5} source={sensor_time:>10} "
              f"raw=[{values}]", flush=True)
        print(f"          transport-gap={lane['transport_gaps']} "
              f"transport-repeat={lane['transport_repeats']} "
              f"RF-gap={lane['rf_gaps']} RF-repeat={lane['rf_repeats']} "
              f"max-silence={lane['max_silence_s']:.3f}s", flush=True)
    crc_status = "OK" if state["blocks"] else "WAIT"
    print(f"  CRC={crc_status} USB-gap={state['usb_gaps']} block-gap={state['block_gaps']}"
          + (" END" if final else ""), flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM19")
    parser.add_argument("--seconds", type=float, default=0,
                        help="0 means continuous until Ctrl+C")
    parser.add_argument("--output-root", type=Path,
                        default=Path("evidence") / "dual_rx_console")
    args = parser.parse_args()
    if args.seconds < 0:
        parser.error("seconds must be zero or positive")
    output = args.output_root / datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    output.mkdir(parents=True, exist_ok=False)
    metrics = StreamState()
    decoder = UsbStreamDecoder()
    alignment = None
    error = None
    last_print = -1.0
    print("LIVE DUAL RX USB DEMO | TX1 -> RX1/RX2 -> FPGA -> WCH -> PC")
    print(f"USB={args.port} | one TX1 source, two physical receivers | Ctrl+C to stop")
    print(f"SAVE={output}", flush=True)
    try:
        with serial.Serial(args.port, 115200, timeout=0.1) as port, \
                (output / "raw_usb.bin").open("wb") as raw, \
                (output / "validated.ubr1").open("wb") as saved:
            while args.seconds == 0 or time.monotonic() - metrics.started < args.seconds:
                chunk = port.read(max(1, min(port.in_waiting, 16384)))
                raw.write(chunk)
                for frame in decoder.feed(chunk):
                    if alignment is None:
                        alignment = decoder.discarded_bytes
                    if metrics.ingest(frame):
                        saved.write(frame.wire)
                elapsed = time.monotonic() - metrics.started
                if elapsed - last_print >= 1:
                    show(metrics.snapshot())
                    saved.flush()
                    last_print = elapsed
    except KeyboardInterrupt:
        print("\nStopped by user.", flush=True)
    except Exception as exc:
        error = f"{type(exc).__name__}: {exc}"
        print("ERROR: " + error, flush=True)
    finally:
        report = metrics.snapshot()
        report["error"] = error
        report["source"] = "TX1"
        report["mode"] = "same_tx_dual_rx"
        report["alignment_bytes"] = alignment if alignment is not None else decoder.discarded_bytes
        report["midstream_discarded"] = max(
            0, decoder.discarded_bytes - alignment) if alignment is not None else 0
        report["passed"] = (
            error is None and report["usb_gaps"] == 0 and report["block_gaps"] == 0
            and report["midstream_discarded"] == 0 and report["payload_mismatches"] == 0
            and all(lane["records"] > 0 for lane in report["lanes"]))
        show(report, final=True)
        (output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"SAVED={output} PASSED={report['passed']}", flush=True)
    return 0 if error is None and (args.seconds == 0 or report["passed"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
