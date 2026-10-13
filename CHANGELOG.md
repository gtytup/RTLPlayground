# Change Log

## [0.x] - 2026-xx-XX

## Added

- Devices
  - Support the hongyavision HYWS-SGT0204S (PCB marking `2G5F4_10G2_V1.01`), an
    RTL8372 based 4x2.5G + 2xSFP+ unmanaged switch with a 512 KB Bright Moon
    `T25S40` flash. The two SFP+ cages are wired the opposite way round compared
    to the GW-9000-6XH-X2 V1.1 (physical 5 is on SDS1, physical 6 on SDS0), the
    serial header silkscreen labels are wrong, and `LED_GLB_IO_EN` bit 6 has to
    be set for the port LEDs to work. New machine target
    `MACHINE_2G5F4_10G2_V1_01`, see `doc/devices/2G5F4_10G2_V1.01.md`.

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

- Web UI
  - A new connection resets the transfer state, so a reused connection slot can no
    longer answer the next request from the middle of the previous response.
  - `/information.json` no longer reads the EEPROM of an empty SFP cage; the CLI and
    the background poll already checked the module-detect pin.

- Management plane
  - Rapid HTTP connection churn could park the NIC receive buffer for good. The main
    loop drained the buffer only while the receive interrupt was raised, so once the
    NIC stopped raising it the switch stopped answering ICMP, the web UI and the
    console while the ASIC kept forwarding, and only a power cycle brought it back.
    The buffer is now drained once per system tick and the NIC is reset if it stays
    full for five seconds.
  - The I2C transfer wait is bounded, so a module holding the bus costs a failed read
    instead of the whole management plane.

- System
  - `strcpy()` was declared in `rtl837x_common.h` but no longer implemented since
    `494320b`, so calls bound to SDCC's library version, which takes the destination
    as a generic pointer while the declaration promises `__xdata`. The only caller is
    the default-hostname path, so a switch without a `hostname` line in its config
    came up with an empty hostname. Upstream fixed this independently in `c773230`
    ("Bring back the definition of strcpy"); this branch had its own implementation
    (`5ae99d8`) and now uses the upstream one, keeping only the comment that explains
    the ABI mismatch. Merging `logicog:main` had left both definitions in
    `rtlplayground.c`, which made the build fail with `error 65`.

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