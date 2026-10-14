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

**No UART pads are exposed on this board.** The sibling boards bring the three
console through-holes out at the board edge; this one does not have them, so the
stock console is not usable for bring-up. (The SoC pins would be GPIO31 = UART
TX and GPIO32 = UART RX, `RTL8372` balls L20/L21 per `doc/gpio.md`, if soldering
to the package is ever considered.) Everything after flashing is done over the
RTLPlayground web UI instead, see *Console access* below.

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

### Measured register state

Taken on the board over the web CLI, running `v0.1.0-682a494`
(`Hardware: IES-WZ0204-V1.1`).

| Command | Result | Reading |
|---------|--------|---------|
| `version` | `IES-WZ0204-V1.1` | the target boots; `flash_sio = 1` is the right setting |
| `regget 0004` | `0x83720000` | **RTL8372, non-`N`** — same decoding the firmware uses: `sfr_data[1] == 0x72` → not 8373, `sfr_data[2] == 0x00` → not `N`. Matches `isRTL8373 = 0` |
| `leds`, `LED_GLB_IO_EN` | `0x77ffff40` | bit 6 set, i.e. `machine_custom_init()` is applied. Pads 8–26 and 28–30 are enabled, 27 and 31 are not |
| `leds`, `PIN_MUX_0` | `0x28000000` | bits 27 and 29, matching `.high_leds.mux = LED_27 \| LED_29` |
| `regget 7f90` | `0x40000041` | **bit 0 = 1: the UART0 TX function is muxed onto GPIO31.** RTLPlayground sets this bit itself (`rtlplayground.c`, "Enable serial interface"), so GPIO31 is the live serial console pin |
| `gpio` twice | GPIO34 and GPIO42 toggled | noise baseline: `MDC0` is polled by the PHY code and GPIO42 is an SPI pin. Any bit that differs between two back-to-back `gpio` calls is not a switch |

### GPIO31 and the VLAN switch

The VLAN switch sits on a pin the firmware drives: with the switch down, the UART0
TX output is pulled to ground, and while the UART transmits, the input readback
follows the transmitted data instead of the switch. So the pin cannot be sampled
reliably at an arbitrary moment, and shorting a driven output is a hardware
concern in itself.

Options, none of them free — except that the board has **no serial console exposed
at all** (no through-holes; the only way in is the web UI), which changes the
trade-off:

1. **Read GPIO31 anyway**, requiring two reads to agree so a UART frame in
   flight is discarded. Keeps the UART alive for anyone who later solders to the
   package. Leaves the electrical question open: with the switch closed, every
   transmitted byte is shorted to ground.
2. **Clear `PIN_MUX_1` bit 0** so the pin is a plain GPIO and the UART no longer
   drives it. Because the serial console is unreachable on this board anyway,
   this costs nothing in practice — the web UI is unaffected — and it removes
   both the readback conflict and the shorting concern. **Preferred.**
3. Leave the VLAN switch unimplemented and document it as such.

The 10G switch needs no such decision, see above: no GPIO tracked it.

### SFP LEDs: this board has two per cage

The sibling `2G5F4_10G2_V1.01` has a single LED per SFP cage. This board has
**two**, and only one of them (the activity one) is doing anything, so the LED
configuration needs a second entry driven for the SFP ports.

Ports 3–8 all use LED-set 0, whose four entries are currently:

| LED-ID | condition | on the sibling |
|--------|-----------|----------------|
| 0 | 2G5 \| 1G \| 100M \| 10M \| LINK \| ACT \| 10G | copper blue |
| 1 | 2G5 \| LINK \| 10G | copper white |
| 2 | 1G \| LINK | the single SFP LED |
| 3 | 0 (never lights) | — |

So the second SFP LED is most likely LED-ID 3, which is programmed to never
light.

That is not just a guess. At a linked SFP the other three entries are all true,
so each of their pads must already be lit if any pad is wired to them:

| LED-ID | at a linked SFP | pad would be |
|--------|-----------------|--------------|
| 0 | LINK \| ACT \| 10G | lit, blinking |
| 1 | LINK \| 10G | lit, solid |
| 2 | LINK (1G is false at 10G) | lit, solid |
| 3 | 0 | **dark** |

One LED blinks and the second is completely dark, which matches LED-ID 3 exactly.

### LED configuration encoding

Derived from `rtl837x_leds.c` (`leds_setup`) and `rtl837x_leds_dump.c`; useful for
any further LED work on this board.

Each LED-ID is a 20-bit value: a 16-bit condition word plus a 4-bit high-speed
nibble in bits 16–19.

| Register | Holds |
|----------|-------|
| `LED1_0_SETn` (`0x6548 - n*8`) | `[LED-ID1 high][LED-ID1 low][LED-ID0 high][LED-ID0 low]` |
| `LED3_2_SETn` (`0x6544 - n*8`) | `[LED-ID3 high][LED-ID3 low][LED-ID2 high][LED-ID2 low]` |
| `LED3_0_SET1` (`0x6528`) | nibbles for SET0/SET1, `[set1 LED2/3][set1 LED0/1][set0 LED2/3][set0 LED0/1]` |
| `LED3_0_SET3` (`0x6524`) | same for SET2/SET3 |

Within each nibble byte, the low nibble is the even LED-ID and the high nibble
the odd one. Nibble bits: 0 = 10G, 1 = TWO_5G, 2 = 5G, 3 = TWO_2G5.

Condition word bits, from `rtl837x_leds.h`: 0 2G5, 1 TWO_1G, 2 1G, 3 500M, 4 100M,
5 10M, 6 LINK, 7 LINK_FLASH, 8 ACT, 9 RX, 10 TX, 11 COL, 12 DUPLEX, 13 TRAINING,
14 MASTER.

The `regset` command writes the value big-endian into `SFR_DATA_24..0`, so
`regset 6544 00400044` puts `0x0040` into LED-ID3 and leaves LED-ID2 at `0x0044`
(1G \| LINK).

### Still to decide on the mode switches

- **GPIO31 / UART**: options 1–3 above, unresolved.
- **GPIO15**: whether the 10G switch is currently down (reads 0).

### LED pad map

The 6-bit field in `LED_GLB_MUX_1..6` decodes as **`LED-ID = (port << 2) | index`**,
with `port` the LED port number (3–8 for this family) and `index` 0–3. That fits
the dump exactly and explains pads 0–7 reading `0x3f` (unassigned): LED ports 0–2
do not exist on a 4+2 board.

| LED port | pads (index) |
|----------|--------------|
| 3 | 8 (3), 9 (0), 10 (1), 11 (2) |
| 4 | 12 (0), 13 (1), 14 (2) |
| 5 | 15 (0), 16 (1), 17 (2) |
| 6 | 18 (0), 19 (1), 20 (2) |
| 7 | 21 (0), 22 (1), 23 (2) |
| 8 | 24 (0), 25 (1), 26 (2), 27 (3) |

Assuming the LED port number is the logical port number, LED port 3 is the right
SFP cage (logical 3) and LED port 8 the left one (logical 8); LED ports 4–7 are
the four copper ports.

**Only one port pad is disabled in `LED_GLB_IO_EN`** (`0x77ffff40`):

| Register bits | Pads |
|---------------|------|
| enabled | 6, 8–26, 28, 29, 30 → `0x77ffff40` |
| **disabled** | 0–5, 7, **27**, 31 |

Pad 27 is LED port 8, index 3 — the fourth slot of the left SFP cage. Every other
port pad is enabled. That makes pad 27 the prime candidate for the second SFP LED
that stays dark: it cannot light no matter what its LED-ID condition says.

Live test, no reflash (`RTL837X_REG_LED_GLB_IO_EN` is `0x65DC`):

```
regset 65dc 7fffff40     # enable pad 27
regset 6548 01400140     # SET0: LED-ID0 and LED-ID1 = LINK | ACT
regset 6544 01400140     # SET0: LED-ID2 and LED-ID3 = LINK | ACT
```

With all four entries set to LINK \| ACT, every wired LED on a linked port must
blink. If the second SFP LED comes alive, narrow down which index drives it:

| Test | Commands |
|------|----------|
| index 0 only | `regset 6548 00000140` then `regset 6544 00000000` |
| index 1 only | `regset 6548 01400000` then `regset 6544 00000000` |
| index 2 only | `regset 6548 00000000` then `regset 6544 00000140` |
| index 3 only | `regset 6548 00000000` then `regset 6544 01400000` |

If it stays dark even with all four driven and pad 27 enabled, the second SFP LED
is not driven by LED-set 0 at all and the search has to move elsewhere.

Reboot to restore the compiled configuration.

### Mode switches (DIP)

**Settled: GPIO15 is a port LED pad, not the 10G switch.**

GPIO15 is LED pad 15 = LED port 5 index 0, i.e. one of the two LEDs of a copper
port. The reading that settles it is the cable move: with the management cable on
`port2` the LED ports involved are 5 and 6, and moving the cable to `port3`
changed exactly four bits:

| Bit | Pad | Belongs to |
|-----|-----|------------|
| 15 | LED port 5 idx 0 | copper port, first LED |
| 16 | LED port 5 idx 1 | copper port, second LED |
| 18 | LED port 6 idx 0 | the other copper port, first LED |
| 19 | LED port 6 idx 1 | the other copper port, second LED |

Four pads = two ports × two LEDs, and only those four moved. That is the cable
moving from one port to the other, with no coincidence left over. In the same
round GPIO15 and GPIO16 sat at opposite levels, which is what two LEDs of one
port do, and GPIO15 toggled while the cable was on its port — the activity LED
blinking with the CLI traffic it was itself carrying.

So GPIO15 cannot be used as a switch input, and the earlier 1:1 correlation with
the 10G switch was an artifact.

**Consequence: the 10G switch may not be readable at all.** In neither round did
any pin track the switch flips. If it is a hardware strap that only the ASIC
samples at reset, there is nothing for the firmware to read and nothing to
implement — RTLPlayground would already honour it. A functional check: insert a
1G module and flip the switch; if the link drops when the switch is engaged, the
strap is live and no firmware work is needed.

**GPIO31 stands as the VLAN switch.** It is the UART0 TX pin, and the switch is
the only thing that can pull it low, so it is readable while the UART is idle.

One remaining question about reading the VLAN switch, since GPIO31 is a pin
RTLPlayground itself drives:

- **GPIO31 is `UART0_TX`.** RTLPlayground runs the console on it and enables the
  UART mux itself (`PIN_MUX_1` bit 0 is set, confirmed with `regget 7f90`), so a
  closed switch shorts the SoC's UART output to ground, and while the UART is
  transmitting the input readback follows the transmitted data instead of the
  switch.

The GPIO15 uncertainty is resolved above: it is an LED pad, not the 10G switch.

Neither pin collides with the SFP pins used by the target (GPIO30/37/50/10) or
its I²C pins (GPIO39/40/41), so the SFP detection is unaffected either way.

One caution on the method: a fourth reading taken while resetting both switches
additionally showed GPIO32, GPIO34 and GPIO42 moving (UART0 RX floating, MDC0
polled by the PHY code, GPIO42 a SPI pin). Always take **two `gpio` readings
back to back with the switches untouched** and ignore any bit that differs
between those two — those are floating or polled pins, not switches.

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

### Console access

This board has no serial header, but it does not need one: RTLPlayground's web
UI contains a full CLI console.

1. Plug a PC into any RJ45 port and set the PC to `192.168.10.0/24`.
2. Browse to `http://192.168.10.247` — the default address from `config.txt`,
   which is baked into the image at `0x6f000`/`0x70000`.
3. Log in. The default password is `1234`.
4. The overview already shows what is needed for most of the checklist: the
   Ports card gives link state and speed per logical port, and the SFP slots
   show module presence, vendor and DDM values.
5. For everything else, the System card has a **`CLI command...`** box with a
   **Send** button. Whatever is typed there is run by the switch's CLI and the
   output is echoed into the log pane underneath. Every "run *cmd*" below means
   typing it into that box. This includes `gpio`, `sfp`, `leds`, `port <n> show`,
   `stat`, `regget`.

The same is available over HTTP for scripted runs:

```sh
SW=192.168.10.247
curl -c cookies.txt http://$SW/login -d pwd=1234        # session cookie
curl -b cookies.txt -X POST http://$SW/cmd --data 'gpio'
curl -b cookies.txt http://$SW/status.json              # port + SFP status
```

Since the board has no serial console, the boot log itself (the `Detecting
CPU:`, `Hardware:` and flash-identification lines) cannot be watched. The
information that mattered in those lines is still reachable:

| Boot-log line | Web-UI equivalent |
|---------------|-------------------|
| `Detecting CPU: RTL837x` | run `regget 0004` and decode, see step 4 below |
| `Hardware: <machine name>` | `hw_ver` on the overview page — must read `IES-WZ0204-V1.1` |
| flash identification | if the UI comes up at all and the CLI answers, `flash_sio = 1` is right; a wrong value garbles or kills the boot |
| `Flash too small for updating` | expected, nothing to check: 512 KB cannot hold the upload image |

### Bring-up checklist

**1. Back up the flash** with a SOIC-8 clip and a CH341A (or similar), reading
1 MiB. Use `flashprog`/IMSProg/NeoProgrammer: mainline flashrom's Boya table
does not contain `0x4013`, so plain `flashrom` will report an unknown chip.

**2. Write the RTLPlayground image** (512 KB, exactly fills the flash), then
**remove the clip before applying power**. Leaving the clip attached while
powering the board has been observed to hold CS/CLK and produce a completely
dead board.

**3. Get to the web UI** as described under *Console access* above and confirm
`hw_ver` reads `IES-WZ0204-V1.1`.

**4. SoC variant.** Run `regget 0004` (that is `RTL837X_REG_CHIP_ID`) and read
the printed value the same way the firmware does:

| Value | Meaning |
|-------|---------|
| `0x8372....` | RTL8372 |
| `0x8373....` | RTL8373 |
| `0x..70..` | the `N` variant |

So `0x83727000` would be an RTL8372N. The machine target currently declares
`isRTL8373 = 0`.

**5. Copper port order.** Plug a live link into one physical RJ45 at a time and
watch which logical port goes `up` on the Ports card (or poll `port <n> show`).
Record physical→logical for all four.

**6. SFP cages.** Insert a module into the left cage and run `gpio`. The GPIO
input bit that flips is that cage's `pin_detect`. Repeat for the right cage.
Then check the SFP slots in the UI: if a cage is reported under the wrong slot
number, swap `sfp_port[0]`/`sfp_port[1]` (i.e. `log_to_phys_port` and
`is_sfp`). Which SDS a cage is wired to cannot be measured with GPIO — it is
determined by linking up: if the left cage never links while the right one
does, swap the two `.sds` values. A slot showing a module's rate and encoding
confirms the I²C bus assignment.

**7. Mode switches.** Flip one switch at a time and run `gpio`. The output is
two lines:

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

**8. LEDs.** With a link up, `leds` dumps `RTL837X_REG_LED_GLB_IO_EN`, the
LED-set configuration and the LED pad configuration. Use that plus the observed
colour to confirm the LED-ID → colour mapping (white/blue/SFP) and that
`LED_GLB_IO_EN` bit 6 really has to be set here as it does on the sibling
board. `machine_custom_init()` currently sets that bit for this machine.

**9. Reset pin.** Check `gpio` at boot: if a GPIO reads permanently low, it is
a candidate for a reset button footprint. `reset_pin` stays `GPIO_NA` until
one is confirmed.

**10. Fold the results back** into `machine.c`, update this document, drop the
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
