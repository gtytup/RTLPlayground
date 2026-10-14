IES-WZ0204-V1.1
================

**Status: bring-up in progress. The port, SFP and LED mapping documented below
is a starting hypothesis inherited from `2G5F4_10G2_V1.01` and is NOT verified
on this board yet.** See the bring-up checklist at the end.

RTL8371B-family based industrial 4×2.5G + 2×SFP+ unmanaged switch.

### Board markings

- PCB silkscreen: `IES-WZ0204-V1.1`
- No public product page found for this model number, so the retail label is
  unknown. It looks like a white-label/industrial part.

The PCB and the component selection differ from the `2G5F4_10G2_V1.01`
(HYWS-SGT0204S) board of the same class, presumably for an extended temperature
rating. The two boards share the chip family, the port count and the stock
firmware family, but nothing about the wiring may be assumed.

### Ports

- 4 × RJ45: 10/100/1000/2500 Mbps
- 2 × SFP+: 1G / 2.5G / 10G

### Stock firmware

A factory test console, not a management CLI. Same program as on the
`2G5F4_10G2_V1.01` and `HYWS-SGT0204S` boards — a Realtek IOL/SDS production
test menu:

- UART **9600** baud, 8N1, 3.3V TTL
- Banner `RTL8371B start`, prompt `RTL8371B:`
- `?` prints the IOL port-pattern / SDS PRBS / eye-monitor menu
  (includes `chip sel:8373/8372: 3, 8224: 4`)
- Interrupts forwarding, so never run it on a live device

Dump (512 KB, full chip read):

| Item | Value |
|------|-------|
| SHA-256 | `b187499df83bd472003f330acc5c550faaa85760ca68a87d5cebd817929c2b08` |
| Size | 524288 B |
| Content | `0x00000`–`0x13699`, everything above is `0xFF` |
| Header | `00 40` RTL8371B firmware magic, 8051 code starts at `0x00002` |

The dump carries **no board configuration** at all: no board name string, no
MAC address, no port/LED/GPIO table. It is byte-for-byte the same *source* as
the stock firmware of the sibling boards, compiled a second time (the sibling
build embedded source line numbers literally in its debug strings —
`698,retval=%d,…` — this build uses a runtime `%d`). Consequently the port
order, SFP wiring and LED mapping cannot be derived from it and have to be
measured.

### Stock flash

| Item | Value |
|------|-------|
| Chip | Boya/BoHong `BY25D40` (JEDEC ID `0x68 0x40 0x13`) |
| Capacity | 4 Mbit = 512 KB, SOP-8 |
| Also known as | `B.25D40` / `B.25D40AS`, `ACE25QA512G`, `LR25D40S` |

The top marking was read as `8L00LJ3 / YK803`, which does not match Boya's
part-number marking style; it is most likely a lot/date code or a differently
oriented marking. The JEDEC ID read back by the programmer is authoritative.

**Read the chip as 1 MiB, not 512 KiB, for the backup.** If the fitted part
were actually 8 Mbit, reading only 512 KiB would silently truncate the dump.

### flash_sio = 1 is required, not just a fallback

A fit of `BY25D40` exposes `0x03` (read), `0x0B` (fast read) and `0x3B` (dual
*output* fast read) but **no `0xBB` dual-I/O fast read**. RTLPlayground's
`flash_init(1)` configures `CMD_FREAD_DIO = 0xBB`, so DIO code fetch cannot work
on this chip. The machine target therefore sets:

```c
.flash_sio = 1,
```

which selects `CMD_FREAD = 0x0B`. Do not "optimize" this away without
re-checking the datasheet revision.

### Port layout, serial console, LEDs, SFP GPIOs

**All unverified.** The machine target currently inherits the
`2G5F4_10G2_V1.01` values:

| Item | Hypothesis (from 2G5F4_10G2_V1.01) |
|------|--------------------------------------|
| Copper physical 1–4 | logical 4, 5, 6, 7 |
| Left SFP cage (physical 5) | logical 8, SDS1, detect GPIO30, LOS GPIO37, I2C SDA4/SCL3 |
| Right SFP cage (physical 6) | logical 3, SDS0, detect GPIO50, LOS GPIO10, I2C SDA3/SCL3 |
| LED-ID0 / LED-ID1 / LED-ID2 | blue / white / SFP |
| reset_pin | `GPIO_NA` (no populated button seen) |

### Mode switches (DIP)

The case has **two slide switches: one labelled VLAN, one labelled 10G.**
RTLPlayground does not read them today, so with the regenerated firmware they
would be dead controls — the stock firmware presumably honours them.

Inferred behaviour, from a same-class 4×2.5G + 2×SFP+ industrial switch (not
verified on this board — confirm against the case silkscreen):

| Switch | Position | Effect |
|--------|----------|--------|
| VLAN | ON | port isolation: RJ45 1–4 form one group and the two SFP+ another, no cross-group forwarding |
| VLAN | OFF | all six ports in one broadcast domain (plain unmanaged switch) |
| 10G | ON | the SFP+ SerDes are forced to 10G |
| 10G | OFF | 1.25G / 10G adaptive, per the module's EEPROM rate |

Two reasons this matters beyond feature parity:

1. **They occupy GPIOs.** Any pin driven by a switch must not be reused as an
   SFP `pin_detect` / `pin_los`. The inherited target uses GPIO30, GPIO10,
   GPIO37 and GPIO50 for the two cages, so if a switch sits on one of those,
   SFP detection silently breaks. The unused `ACL_BIT0_EN` / `ACL_BIT1_EN` /
   `ACL_BIT2_EN` names (GPIO52–54) are the kind of pins these switches are
   usually wired to.
2. **The stock switch behaves differently depending on them**, so a user
   flipping a switch on the regenerated firmware would see no change at all.

**Policy: one-directional.** When a switch is ON the firmware forces the
corresponding behaviour; when it is OFF the firmware leaves the configuration
alone. A switch position therefore never permanently shadows a saved
configuration, but flipping one always has a visible effect.

Implementation plan (to be executed once the two GPIOs are measured):

1. Two fields in `struct machine`, e.g. `pin_mode_vlan` and `pin_mode_10g`,
   defaulting to `GPIO_NA` = not fitted. The feature is inert while both are
   `GPIO_NA`, which is the state of the current image.
2. Read them **after `execute_config()`** in `main()`, not in
   `machine_custom_init()`. The isolation masks are first written by
   `port_l2_setup()` and then re-written by the config replay, so the switch has
   to be the last writer. `machine_custom_init()` runs far too early.
3. VLAN = ON: apply the groups with `port_isolate()`. Copper logical ports and
   SFP logical ports each form their own group and every mask keeps `PMASK_CPU`
   so management stays reachable. Derive the two groups from
   `machine.is_sfp[]`, do not hard-code them, so the grouping follows whatever
   the port mapping turns out to be.
4. 10G = ON: make `sfp_rate_to_sds_config()` in `sfp.c` return `SDS_10GR`
   unconditionally, and re-run the SDS setup for modules that are already
   inserted when the switch is sampled.
5. Print one line at boot (`Mode switch: VLAN=on 10G=off`) so it is visible what
   was applied.

### Machine target

```
MACHINE_IES_WZ0204_V1_1
```

### Bring-up checklist

Everything below is done over the RTLPlayground console at 115200 8N1.

**1. Find the UART pads.** Do not trust the silkscreen; on the sibling board
the three labels were shifted by one. Identify ground by continuity, then find
the board's TX pin by testing which of the remaining two pads prints when wired
to the adapter's RX. The pads are 3.3V TTL; never connect VCC.

**2. Confirm the stock firmware** at 9600 8N1: expect `RTL8371B start` and the
`RTL8371B:` prompt, `?` prints the test menu.

**3. Back up the flash** with a SOIC-8 clip and a CH341A (or similar), reading
1 MiB. Use `flashprog`/IMSProg/NeoProgrammer: mainline flashrom's Boya table
does not contain `0x4013`, so plain `flashrom` will report an unknown chip.

**4. Write the RTLPlayground image** (512 KB, exactly fills the flash), then
**remove the clip before applying power**. Leaving the clip attached while
powering the board has been observed to hold CS/CLK and produce a completely
dead board.

**5. Record the boot log** at 115200 and check:

| Console line | What it settles |
|--------------|-----------------|
| `Detecting CPU: RTL837x` | exact SoC variant (8372 / 8372N / 8373 / 8373N) |
| `Hardware: IES-WZ0204-V1.1` | the right machine target is running |
| `Manufacturer ID` / `Memory Type` / `Capacity` | flash identification; expects `0x68 0x40 0x13` and `512 KB`. Garbled output here means `flash_sio = 1` was wrong |
| `Flash too small for updating` | expected: 512 KB leaves no room for the Web update image |

**6. Copper port order.** Plug a live link into one physical RJ45 at a time and
watch the console (`<new link: …>` reports the logical port) or poll
`port <n> show` for each logical port. Record physical→logical for all four.

**7. SFP cages.** With the console running, insert a module into the left cage
and issue `gpio`. The GPIO input bit that flips is that cage's `pin_detect`.
Repeat for the right cage. Then run `sfp`: if a cage is reported under the
wrong slot number, swap `sfp_port[0]`/`sfp_port[1]` (i.e. `log_to_phys_port`
and `is_sfp`). Which SDS a cage is wired to cannot be measured with GPIO — it
is determined by linking up: if the left cage never links while the right one
does, swap the two `.sds` values. `sfp` printing a module's rate and encoding
confirms the I²C bus assignment.

**8. Mode switches.** With the console running, flip one switch at a time and
issue `gpio`. The output is two lines:

```
GPIO 0: <8 hex digits> <8 hex digits>
GPIO 1: <8 hex digits> <8 hex digits>
```

- `GPIO 0` is `RTL837X_REG_GPIO_00_31_INPUT` (0x44), `GPIO 1` is 0x48
  (GPIO 32–63).
- The first group of digits is the current level, most significant bit first:
  for bank `b` (0 → base 0, 1 → base 32) digit `i` counted from the left covers
  `base + 31 - 4i` … `base + 28 - 4i`. So GPIO37 is the 7th digit of `GPIO 1`.
- The second group is the XOR against the previous `gpio` invocation, i.e.
  exactly the pins that changed since it. Flip a switch, re-issue `gpio`, and
  the pins that moved are the ones you want.

Record both pins and both positions. Two things to watch out for:

- If a pin is driven as an **output** by the firmware, its input level says
  nothing about a switch. A value that does not track the switch at all is a
  hint that this is the case.
- If **neither** pin moves, the switches are probably hardware straps that are
  only sampled at reset and are not visible as GPIOs. Then the feature can only
  be honoured by reading the resulting ASIC register state instead of a GPIO
  level, and the plan above needs adjusting.

Finally, confirm the ON/OFF semantics against the case silkscreen — the table
above is inferred from a same-class product, not measured here.

**9. LEDs.** With a link up, `leds` dumps `RTL837X_REG_LED_GLB_IO_EN`, the
LED-set configuration and the LED pad configuration. Use that plus the observed
colour to confirm the LED-ID → colour mapping (white/blue/SFP) and that
`LED_GLB_IO_EN` bit 6 really has to be set here as it does on the sibling
board. `machine_custom_init()` currently sets that bit for this machine.

**10. Reset pin.** Check `gpio` at boot: if a GPIO reads permanently low, it is
a candidate for a reset button footprint. `reset_pin` stays `GPIO_NA` until
one is confirmed.

**11. Fold the results back** into `machine.c`, update this document, drop the
"unverified" banner and only then add a row to `doc/supported_devices.md`.

### Building

```sh
docker run --rm -v $PWD:/workspace -w /workspace rpg-build:latest \
    make MACHINE=IES_WZ0204_V1_1 -j4
```

In a `git worktree` the build needs two extra variables. A worktree's `.git` is a
file pointing at an absolute path inside the main checkout, which does not
exist under the container's `/workspace`, so `git rev-parse` fails there: the
file name loses the version (`v0.1.0--IES_WZ0204_V1_1.bin`) and
`SOURCE_DATE_EPOCH` falls back to the wall clock, making the image
non-reproducible.

```sh
EPOCH=$(git show -s --format=%ct HEAD)
docker run --rm -v $PWD:/workspace -w /workspace -e SOURCE_DATE_EPOCH=$EPOCH \
    rpg-build:latest make MACHINE=IES_WZ0204_V1_1 -j4 VERSION_EXTENSION=v0.1.0-$(git rev-parse --short HEAD)
```

Note that exactly one `MACHINE_` define may be active in `machine.h`: the
`#if/#elif` chain in `machine.c` takes the first match, so a left-over
`#define MACHINE_GW_9000_6XH_X2_V1_1` silently overrides `make MACHINE=…`.
