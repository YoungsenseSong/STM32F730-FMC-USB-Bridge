# Specification

## 1. Objectives

| Metric | Target | Method | Current result | Status |
|---|---:|---|---|---|
| FPGA identity | `0x5343` / protocol 1 | FSMC identity read | Checked in bridge | ☑ |
| Block and record validation | All accepted frames CRC-valid | Host decoder/tests | 15 s real stream passed | ☑ |
| USB stream sequence | Continuous UBR1 sequence | Host capture | 331-frame window passed | ☑ |
| Long-term losslessness | No unexplained gaps | Extended board test | Not established | ☐ |

## 2. Functional requirements

- Read FPGA block header and payload from multiplexed FSMC.
- Validate block, record header, and record payload CRCs.
- Retry transient reads where configured.
- Send UBR1 frames to USB FS CDC and ACK an accepted block.

## 3. Interfaces

| Interface | Direction | Width | Protocol/level | Notes |
|---|---|---:|---|---|
| FSMC AD | Bidirectional | 16 | 3.3 V multiplexed | WCH master, FPGA slave |
| NADV/NE/NOE/NWE | WCH to FPGA | 4 | 3.3 V | See `WIRING_WCH_FSMC.md` |
| NWAIT | FPGA to WCH | 1 | 3.3 V active low | Verify electrical polarity |
| USB FS CDC | Bidirectional | USB | USB full speed | UBR1 framed data |

## 4. Clock and reset

Configure WCH system clock, USB clock, and FSMC timing from the target board
configuration. After FPGA reconfiguration, re-check identity and acquisition
state before consuming blocks.

## 5. Resource and environment

- Build with CH32V307VCT6-specific WCH toolchain.
- Keep static RAM and reserved stack within the 64 KiB SRAM budget.
- Do not include build cache, local tool paths, or raw hardware logs in Git.

## 6. Acceptance

1. CMake build and host unit tests pass.
2. Real FPGA payload passes USB, block, record and sequence checks.
3. Long-duration and recovery checks are reported separately.
