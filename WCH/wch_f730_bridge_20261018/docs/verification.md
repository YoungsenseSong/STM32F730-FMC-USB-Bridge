# Verification

## 1. Environment

- Date: 2026-10-18
- MCU: CH32V307VCT6
- Toolchain: RISC-V GCC12 / CMake / Ninja
- FPGA: current dual-channel ZYNQ bitstream

## 2. Host tests

| Date | Test | Result | Evidence |
|---|---|---|---|
| 2026-10-18 | Protocol golden vectors | Pass | `tests/golden/` |
| 2026-10-18 | Bridge state/CRC/sim-source tests | Pass | Build verification record |

## 3. Build and resource

| Date | Tool | Static RAM | Stack reserve | Result |
|---|---|---:|---:|---|
| 2026-10-18 | WCH GCC12/Ninja | 44,512 B | 8 KiB included in budget | WCH bridge build passed |
| 2026-10-19 | WCH GCC12/Ninja | text 24,736 B; data 152 B; bss 44,512 B | 8 KiB included in budget | Clean out-of-tree CMake build passed |

## 4. Board tests

| Date | Test | Method | Result | Conclusion |
|---|---|---|---|---|
| 2026-10-18 | Real dual-RX stream | USB CDC, 15 s | 331 frames; CH0=1955, CH1=1758; CRC/transport errors=0 | Short window passed |
| 2026-10-18 | Silence fault retry | Real board with injected/transient read mismatch | Read retry recovered several blocks | Mitigation observed; electrical root cause open |

## 5. Limitations

The short window is not a long-run lossless claim. RF sequence gaps and FPGA
configuration loss remain distinct from USB framing and FSMC retry behavior.
Raw captures and full logs are not included in this GitHub source publication.
