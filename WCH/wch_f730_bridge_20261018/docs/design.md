# Design

## 1. Purpose

Replace the STM32F730 bridge with a CH32V307VCT6 implementation while
preserving the established FPGA block and UBR1 USB transport contracts.

## 2. Architecture

```text
FPGA 16-bit multiplexed FSMC -> extbus_port -> fmc_bridge
    -> block/record validation -> UBR1 fragmentation -> USB FS CDC -> host
```

## 3. Modules

| Module | Responsibility | Input | Output |
|---|---|---|---|
| `app/extbus_port.c` | FSMC bus access and stable snapshot | FPGA FMC pins | 16-bit words |
| `app/fmc_bridge.c` | Identity, block read, retry, ACK | FSMC words | validated block |
| `protocol/bridge_protocol.c` | CRC and protocol decoding | raw block | records/status |
| `app/usb_port.c` | USB FS CDC transport | UBR1 payload | host CDC stream |

## 4. Decisions

- Preserve FPGA identity `0x5343`, protocol v1, FPB1 block and UBR1 USB framing.
- Treat `flags=0x8` as the known timeout-block flag accepted by the mature F730.
- Keep external bus access gated by identity and block sanity checks.
- Keep the WCH EVT HAL/USB files in a separate vendor directory with original
  notices.

## 5. Timing and reset

FSMC setup, hold, address/data multiplexing, NADV and active-low NWAIT are
board-level interfaces. Validate those waveforms on the actual board. USB
enumeration does not prove FPGA block transport.

## 6. Resource and power

The bridge has a constrained SRAM budget. Read the current linker MAP and keep
stack reserve separate from static `.data`/`.bss`. Power has not been measured.

## 7. Open items

- [ ] Longer continuous WCH board run
- [ ] Repeated handling of transient FSMC read errors
- [ ] Recheck USB/FSMC recovery after FPGA reconfiguration
