# Change Log

## [0.x] - 2026-xx-XX

## Added

- Devices
  - Support the IES-WZ0204-V1.1, an RTL8372 based industrial 4x2.5G + 2xSFP+
    board whose Boya `BY25D40` flash cannot be read in DIO mode, so it needs
    single-IO flash reads. Both cage LEDs sit on an LED entry whose speed bits do
    not include 10G and stayed dark; the LED mux and LED-set are corrected for
    it. Its VLAN slide switch sits on the UART0 TX pin and isolates the copper
    ports from each other while leaving the cages as a shared uplink. New
    machine target `MACHINE_IES_WZ0204_V1_1`, see
    `doc/devices/IES-WZ0204-V1.1.md`.

- Web UI
  - Compress the embedded assets (minify + gzip) and switch to a single-page layout. #315
  - Replace the multi-page UI with a themed single-page app: light, dark and Selenized themes following the browser by default,
    English, Japanese and Chinese, save to flash merges the command log into the startup config and verifies the write,
    firmware images are checked in the browser before upload. #429

## Changed

## Fixed

- Config
  - A startup configuration line longer than the command buffer no longer stops the replay:
    the offending line is skipped and the following lines are still applied.
  - The web UI and the configuration upload now refuse a line the replay cannot take,
    instead of writing it to flash.

## Breaking changes

- Config
  - VLAN don't accept port `u`-suffix anymore.
    So `vlan 1 4u` is not valid anymore.
    Replace it with `vlan 1 4`.
- Commands
  - port zero/`0` is treated as the `CPU_PORT`. #326
  - Many commands don't accept the CPU_PORT anymore. See #334.
    When the CPU_PORT is needed, the command/service will add the CPU_PORT automaticly.
    Only `isolate` accept the CPU_PORT as destination port.

## [v0.1_aplha] - 2025-11-03

First release.