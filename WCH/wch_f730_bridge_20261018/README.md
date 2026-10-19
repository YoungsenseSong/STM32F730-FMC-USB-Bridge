# WCH CH32V307 FSMC to USB bridge

Standalone CH32V307VCT6 bridge firmware that reads FPGA blocks through the
16-bit FSMC interface, validates the shared record/block protocol, and sends
UBR1 frames over USB FS CDC.

- Target MCU: CH32V307VCT6
- Bridge source: `app/` and `protocol/`
- Build presets: `fmc-bridge`, `extbus-on`, `echo`
- Board wiring: `WIRING_WCH_FSMC.md`
- Release image: `release/WchF730Bridge.hex`

The `release/` image is the candidate used by the recorded hardware checks.
The latest 2026-10-18 release SHA256 is
`d2c88216a88e7a7c92bf686f9ec339a3fd45801ba66c660725b5831ed0e34351`.

## Build

Install CMake, Ninja and the WCH RISC-V GCC12 toolchain, then set
`WCH_TOOLCHAIN_ROOT` to the toolchain installation directory. For example:

```powershell
$env:WCH_TOOLCHAIN_ROOT = 'C:\MounRiver\MounRiver_Studio\toolchain\RISC-V Embedded GCC12'
cmake --preset fmc-bridge
cmake --build --preset fmc-bridge
```

`CMakeUserPresets.json` is local-only and intentionally ignored. Use it for
machine-specific paths rather than committing tool cache locations.

## Verification boundary

The repository contains the source, host tests, protocol golden vectors, and
the release image. The 2026-10-18 real FPGA data test observed 331 UBR1 frames
in 15 seconds with CH0/CH1 records and zero USB/block/record CRC or transport
sequence errors. Long-duration stability, RF loss, and physical sampling
synchronization remain separate acceptance items. Raw capture evidence stays
in the local workspace and is not included in this repository.

## Vendor code

The files under `vendor/` and `vendor_usb/` retain WCH copyright notices and
the WCH restriction that the software and binaries are for WCH-manufactured
microcontrollers. See `THIRD_PARTY_NOTICES.md`.
