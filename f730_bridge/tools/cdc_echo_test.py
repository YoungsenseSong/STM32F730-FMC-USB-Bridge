"""STM32 USB CDC loopback stress test.

The script intentionally treats CDC as a bring-up transport only. It verifies
that arbitrary byte streams survive USB packet fragmentation, short reads and
multi-packet bursts; it does not validate the final Vendor Bulk protocol.
"""

from __future__ import annotations

import argparse
import random
import sys
import time
from dataclasses import dataclass
from typing import Protocol


EXIT_OK = 0
EXIT_DEPENDENCY = 3
EXIT_SERIAL = 4
EXIT_TIMEOUT = 5
EXIT_DATA = 6
EXIT_WRITE = 7
EXIT_INTERNAL = 8


class SerialLike(Protocol):
    def write(self, data: bytes) -> int: ...

    def read(self, size: int) -> bytes: ...

    def close(self) -> None: ...


class CdcTestError(RuntimeError):
    exit_code = EXIT_INTERNAL


class WriteError(CdcTestError):
    exit_code = EXIT_WRITE


class ReadTimeout(CdcTestError):
    exit_code = EXIT_TIMEOUT


class DataMismatch(CdcTestError):
    exit_code = EXIT_DATA


@dataclass(frozen=True)
class TestConfig:
    packet_size: int
    rounds: int
    burst: int
    timeout: float
    read_chunk: int
    seed: int


def positive_int(value: str) -> int:
    number = int(value, 0)
    if number <= 0:
        raise argparse.ArgumentTypeError("必须大于 0")
    return number


def positive_float(value: str) -> float:
    number = float(value)
    if number <= 0.0:
        raise argparse.ArgumentTypeError("必须大于 0")
    return number


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="STM32 USB CDC 随机数据、多包突发回环压测"
    )
    parser.add_argument("--port", help="串口名，例如 COM13；实际测试时必填")
    parser.add_argument("--baud", type=positive_int, default=115200)
    parser.add_argument("--packet-size", type=positive_int, default=256)
    parser.add_argument("--rounds", type=positive_int, default=100)
    parser.add_argument(
        "--burst", type=positive_int, default=4, help="每轮连续发送的包数"
    )
    parser.add_argument(
        "--timeout", type=positive_float, default=2.0, help="每轮总接收超时（秒）"
    )
    parser.add_argument(
        "--read-chunk", type=positive_int, default=64, help="每次 read 的最大长度"
    )
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=0x730)
    parser.add_argument(
        "--self-test", action="store_true", help="不打开串口，运行主机侧逻辑自测"
    )
    return parser


def write_all(port: SerialLike, payload: bytes, deadline: float) -> None:
    offset = 0
    while offset < len(payload):
        if time.monotonic() >= deadline:
            raise WriteError(f"写入超时：{offset}/{len(payload)} 字节")
        written = port.write(payload[offset:])
        if written is None:
            written = 0
        if written < 0 or written > len(payload) - offset:
            raise WriteError(f"串口返回非法写入长度：{written}")
        if written == 0:
            time.sleep(0.001)
            continue
        offset += written


def read_exact(
    port: SerialLike, expected_size: int, read_chunk: int, deadline: float
) -> bytes:
    received = bytearray()
    while len(received) < expected_size:
        if time.monotonic() >= deadline:
            raise ReadTimeout(f"接收超时：{len(received)}/{expected_size} 字节")
        request = min(read_chunk, expected_size - len(received))
        chunk = port.read(request)
        if chunk:
            received.extend(chunk)
    return bytes(received)


def first_mismatch(expected: bytes, actual: bytes) -> int | None:
    for index, (wanted, got) in enumerate(zip(expected, actual)):
        if wanted != got:
            return index
    if len(expected) != len(actual):
        return min(len(expected), len(actual))
    return None


def make_packet(rng: random.Random, packet_size: int, sequence: int) -> bytes:
    payload = bytearray(rng.getrandbits(8) for _ in range(packet_size))
    sequence_bytes = sequence.to_bytes(4, "little")
    payload[: min(packet_size, 4)] = sequence_bytes[: min(packet_size, 4)]
    return bytes(payload)


def run_loopback(port: SerialLike, config: TestConfig) -> dict[str, float | int]:
    rng = random.Random(config.seed)
    total_bytes = 0
    started = time.monotonic()

    for round_index in range(config.rounds):
        packets = [
            make_packet(
                rng,
                config.packet_size,
                round_index * config.burst + burst_index,
            )
            for burst_index in range(config.burst)
        ]
        expected = b"".join(packets)
        deadline = time.monotonic() + config.timeout

        for packet in packets:
            write_all(port, packet, deadline)

        actual = read_exact(port, len(expected), config.read_chunk, deadline)
        mismatch = first_mismatch(expected, actual)
        if mismatch is not None:
            wanted = expected[mismatch] if mismatch < len(expected) else None
            got = actual[mismatch] if mismatch < len(actual) else None
            raise DataMismatch(
                f"第 {round_index + 1} 轮数据不一致，偏移 {mismatch}，"
                f"期望 {wanted!r}，收到 {got!r}"
            )
        total_bytes += len(expected)

    elapsed = time.monotonic() - started
    return {
        "rounds": config.rounds,
        "packets": config.rounds * config.burst,
        "bytes": total_bytes,
        "elapsed_s": elapsed,
        "one_way_mib_s": total_bytes / max(elapsed, 1.0e-9) / (1024 * 1024),
        "round_trip_mib_s": 2 * total_bytes / max(elapsed, 1.0e-9) / (1024 * 1024),
    }


class _ChunkedLoopback:
    """Deterministic fake serial port for the no-hardware self-test."""

    def __init__(self, max_write: int, max_read: int) -> None:
        self.max_write = max_write
        self.max_read = max_read
        self.pending = bytearray()
        self.closed = False

    def write(self, data: bytes) -> int:
        count = min(len(data), self.max_write)
        self.pending.extend(data[:count])
        return count

    def read(self, size: int) -> bytes:
        count = min(size, self.max_read, len(self.pending))
        result = bytes(self.pending[:count])
        del self.pending[:count]
        return result

    def close(self) -> None:
        self.closed = True


def run_self_test() -> None:
    config = TestConfig(
        packet_size=67,
        rounds=5,
        burst=3,
        timeout=1.0,
        read_chunk=11,
        seed=0x730,
    )
    fake = _ChunkedLoopback(max_write=7, max_read=3)
    stats = run_loopback(fake, config)
    assert stats["bytes"] == 67 * 5 * 3
    assert stats["packets"] == 15
    assert first_mismatch(b"abc", b"abc") is None
    assert first_mismatch(b"abc", b"abd") == 2
    print("SELF_TEST_OK: short write/read、随机数据和多包突发逻辑通过")


def print_stats(stats: dict[str, float | int]) -> None:
    print(
        "PASS: "
        f"rounds={stats['rounds']} packets={stats['packets']} "
        f"bytes={stats['bytes']} elapsed={stats['elapsed_s']:.3f}s"
    )
    print(
        f"吞吐：单向 {stats['one_way_mib_s']:.3f} MiB/s，"
        f"收发合计 {stats['round_trip_mib_s']:.3f} MiB/s"
    )


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.self_test:
        run_self_test()
        return EXIT_OK
    if not args.port:
        print("错误：实际串口测试必须指定 --port", file=sys.stderr)
        return EXIT_SERIAL

    try:
        import serial
    except ImportError as exc:
        print("错误：缺少 pyserial，请先安装 pyserial", file=sys.stderr)
        print(str(exc), file=sys.stderr)
        return EXIT_DEPENDENCY

    config = TestConfig(
        packet_size=args.packet_size,
        rounds=args.rounds,
        burst=args.burst,
        timeout=args.timeout,
        read_chunk=args.read_chunk,
        seed=args.seed,
    )
    port = None
    try:
        # A short per-read timeout lets read_exact enforce one total deadline.
        read_timeout = min(args.timeout, 0.05)
        port = serial.Serial(args.port, args.baud, timeout=read_timeout)
        print(
            f"连接 {args.port}：packet={args.packet_size} burst={args.burst} "
            f"rounds={args.rounds} timeout={args.timeout:.3f}s seed=0x{args.seed:X}"
        )
        print_stats(run_loopback(port, config))
        return EXIT_OK
    except CdcTestError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return exc.exit_code
    except serial.SerialException as exc:
        print(f"串口错误：{exc}", file=sys.stderr)
        return EXIT_SERIAL
    except Exception as exc:  # pragma: no cover - final safety net for CLI use
        print(f"未处理错误：{type(exc).__name__}: {exc}", file=sys.stderr)
        return EXIT_INTERNAL
    finally:
        if port is not None:
            port.close()


if __name__ == "__main__":
    raise SystemExit(main())
