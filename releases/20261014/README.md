# Current F730 Projects

- `single_channel_demo/`: stable v2 GPIO-serial to USB CDC bridge and Windows
  `SHOW_USB.cmd` live viewer. Real sample records only; limited serial
  throughput and RF sequence gaps remain.
- `single_channel_fmc/`: unchanged latest formal FMC bridge source/image.
  USB initialization is independent of valid FPGA identity; asynchronous
  NWAIT is disabled. Formal FMC board operation remains unverified.

Both projects build using `BUILD_F730.ps1`; clean-built HEX files reproduce
the retained reviewed images byte for byte. `FLASH_F730.py` preserves the
previous manual-reset/DTCM algorithm, exact backup checks, sector programming,
full Flash readback and rollback. It intentionally rejects an unknown current
Flash state. No hardware was programmed during this release.

Firmware is USB CDC, not WinUSB. `tools/vendor_bulk_host.py` is primarily an
offline protocol codec; use SHOW_USB or cdc_capture for the CDC interface.
Original dirty Keil project files are untouched.
