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

### The second SFP LED is probably a module-present indicator

The case has an arrow next to it pointing at the optical port. That reads as a
per-cage indicator, and a `MOD-DEF0` / module-present LED is the usual meaning:
the module pulls `MOD-DEF0` low when inserted, and an LED hung on that net lights
with no firmware involvement at all.

This matters because of what the firmware does with SFP GPIOs: RTLPlayground
drives **one** SFP GPIO as an output, `pin_tx_disable` (`sfp.c`, in
`setup_sfp_gpio()`), and on this target both cages have it set to `GPIO_NA`, so
nothing is driven. There is no firmware feature for a module-present LED.

So there are only two possibilities, and they are easy to tell apart:

| Cause | Behaviour |
|-------|-----------|
| hardware, hung on the module-present net | lights whenever a module is inserted, in either cage, under any firmware |
| a GPIO the stock firmware drives | dark under RTLPlayground by construction — nothing implements it |

**Test:** insert a module into **both** cages and look at both second LEDs.

- Both light → hardware, nothing to implement.
- One or both stay dark → either the stock firmware drove it, or it means
  something else (for example a 10G-speed indicator, which would stay dark with
a 1G/2.5G module).

The answer to "did this LED ever light under the stock firmware" settles it
too, and is worth asking before chasing the pin.

If it does turn out to be firmware-driven, there is a ready hook: `pin_tx_disable`
is already set up as an output during `setup_sfp_gpio()`, and `gpio_output_setup()`
can be reused for a second, dedicated pin. No new plumbing is needed.

### SFP detection is still unverified

Related, and more important than the LED: this target's `pin_detect` (GPIO30 for
the left cage, GPIO50 for the right) and the two I²C buses were inherited from
the sibling board and have **never been verified here**. If a detect pin is
wrong, module recognition silently fails, which would also explain any
recognition-dependent indicator staying dark.

Verify with `gpio` while inserting a module: the bit that flips is that cage's
detect pin. And run `sfp`, which reads the module EEPROM — if it prints a vendor
and model, the detect pin and the I²C bus are both right.

### Why the all-conditions test could not have worked

`LED-ID = (port << 2) | index` is not just a label: it is the **port** whose LED
state the pad displays. A pad assigned to port 7 shows port 7's LED state, not
its own — so a pad whose LED-ID points at a **port without a link** can never
light, whatever conditions are programmed into LED-set 0.

The earlier test set all four conditions of set 0 to `LINK | ACT`, which only
reaches pads assigned to a *linked* port. If this board's SFP LED pads are not
mapped to the SFP LED ports in the ASIC default mux, the bottom LED is
unreachable that way — which fits the observation exactly.

That also implies the fix, if the pad is reachable at all: program the pad→LED-ID
map ourselves (`led_mux_custom = 1` and `led_mux[]`), which no target on this
family has needed so far.

**Reachability test.** Point *every* pad at the LED-ID of a port that certainly
has a link — the management copper port — and every wired LED on the board must
light at once.

```
regset 65dc ffffffff                              # enable all 32 pads
regset 65e0 10410410    # RTL837X_REG_LED_GLB_MUX_1
regset 65e4 10410410    # ... 2
regset 65e8 10410410    # ... 3
regset 65ec 10410410    # ... 4
regset 65f0 10410410    # ... 5
regset 65f4 10410410    # ... 6
```

`0x10410410` sets all five 6-bit fields to `0x10` = LED-ID 16 = port 4 index 0,
assuming the management cable is on the port this target calls physical 1. If it
is on a different port, use that port's LED-ID: `(port << 2) | 0`, packed the same
way (the packing is five 6-bit fields, bit 0 upwards: `v = Σ id << (6k)`; all
`0x3f` reproduces the observed `0x3fffffff`, which is how the packing was
checked).

| Result | Conclusion |
|--------|------------|
| the bottom SFP LED now lights with the rest | its pad is reachable, the pad→LED-ID mux is the answer, and a custom `led_mux` fixes it |
| nothing on the board changes | the pads are not reachable this way; the LED is not an LED-controller pad |

Reboot restores the compiled configuration.

### SFP slot 1 verified

```
> sfp
Slot 1 - Rate: 67  Encoding: 06
FINISAR CORP.   FCBG110SD1C05-1RA
Options: 00
Slot 2 - empty
```

Slot 1 is `sfp_port[0]`, the left cage. The module's EEPROM was read and decoded,
which confirms for the first time on this board that the inherited settings work:

| Setting | Value | Status |
|---------|-------|--------|
| `pin_detect` | GPIO30 | **works** — the module was noticed |
| I²C (SDA4 / SCL3) | GPIO39 / GPIO40 | **works** — vendor and model came back |
| rate 0x67 = 103 × 100 Mbit ≈ 10.3 Gbit | 10G | maps to `SDS_10GR`, so the link is set up as 10G |
| encoding 0x06 | 64b/66b | consistent with 10G |

`FCBG110SD1C05-1RA` is a Finisar 10G SFP+ active optical cable, so the left cage
runs a 10G link.

Two consequences:

- **Slot 2 (right cage) is empty, so its LEDs being dark is correct.** Any LED
  investigation has to happen on the populated cage.
- The right cage's `pin_detect`, `pin_los` and I²C settings remain unverified —
  they are inherited and have never been exercised.

Because slot 1 links at 10G and all four LED-set 0 entries are programmed to
`LINK | ACT` during the investigation, port 8 is a known-true reference and is a
better target for the reachability test below than the copper port.

```
regset 65dc ffffffff                # enable all pads
regset 65e0 21861861                # all pads -> LED-ID 33 = port 8 index 1
regset 65e4 21861861
regset 65e8 21861861
regset 65ec 21861861
regset 65f0 21861861
regset 65f4 21861861
```

Every wired LED on the board should then show port 8's state, i.e. light up
together. If the second SFP LED is among them, its pad is reachable through the
pad→LED-ID mux and a custom `led_mux` is the fix; if nothing changes, it is not
an LED-controller pad at all.

### Pad-group to LED map (measured)

Each `LED_GLB_MUX_n` register carries five pads. Pointing **every pad** at
`LED-ID 33` (port 8, index 1 — a port that is linked at 10G, so the condition is
true) and writing the registers one at a time lights the whole board, register by
register. Result:

| Register | Pads | Physical LEDs that came on |
|----------|------|----------------------------|
| `LED_GLB_MUX_1` `0x65e0` | 0–4 | none |
| `LED_GLB_MUX_2` `0x65e4` | 5–9 | right cage, upper LED |
| `LED_GLB_MUX_3` `0x65e8` | 10–14 | copper 1 both LEDs, right cage lower LED |
| `LED_GLB_MUX_4` `0x65ec` | 15–19 | copper 2 and copper 3, both LEDs each |
| `LED_GLB_MUX_5` `0x65f0` | 20–24 | copper 4 both LEDs, left cage upper LED |
| `LED_GLB_MUX_6` `0x65f4` | 25–27 | left cage lower LED |

The counts reconcile exactly with the hardware: 4 copper ports × 2 LEDs plus 2
cages × 2 LEDs = **12 LEDs**, and the groups above account for 1 + 3 + 4 + 3 + 1
= 12.

Consequences:

- **The second SFP LED is an LED-controller pad and it is reachable.** Forcing its
  pad to another LED-ID lights it, so the fix is a custom `led_mux`, not a
  different driver. This also retires the module-present theory: the right cage's
  upper LED lit with **no module fitted**.
- The pads are **not** grouped the way the ASIC default mux assumes: the LED
  groups cut across `LED_GLB_MUX` registers and mix copper and cage LEDs. That is
  why the default mux, which the sibling board is happy with, cannot drive this
  board's second cage LED.
- The left cage's lower LED is pad 25, 26 or 27.

### Reading the LED pads back

LED pads can be read through `gpio`, which turns the two-state trick into a
reliable probe: set every pad to a true LED-ID, read `gpio`, then set every pad
unassigned (`0x3f`) and read again. The bits that move are driven by the mux.

Measured, with all pads on `LED-ID 33` versus all pads unassigned:

| | Pads reading 0 |
|---|---|
| all pads on a true LED-ID | 0–8, 10, 11, 13, 14, 16, 17, 19–26, 30 |
| all pads unassigned | 9, 12, 15, 18, 25, 26, 30 |

Two things follow:

- **A lit LED means the pad sits low**, i.e. the controller sinks current. Pads
  0–24 flip between the two states, so they really are mux-driven.
- **Pads 25–29 do not flip at all**, and pads 9, 12, 15, 18 sit inverted with
  respect to the rest. The `leds` dump only prints pads 0–27, so **pads 28 and 29
  have never been examined** — and since the `LED_GLB_MUX_6` write *did* light the
  left cage's lower LED, its pad is likely 28 or 29, not 25–27.

**Caveat that invalidated one run:** in the first `gpio` of that pair the change
mask equalled the value, which only happens when `gpio_last_value` is still zero
— i.e. the board had just been rebooted. `LED_GLB_IO_EN` was therefore back at the
compiled `0x77ffff40` and **pad 27 was disabled again**, so anything depending on
pad 27 needs `regset 65dc ffffffff` re-applied and no reboot in between.

### SFP LEDs: solved

**Pad map (measured).** Each LED's pad was found by pointing one pad at a time
at a known-good LED-ID. The ASIC default pad→LED-ID mux already matches this
board — every LED's pad is assigned to its own port:

| LED | Pad | Default LED-ID | (port, index) |
|-----|-----|----------------|---------------|
| copper 1 left / right | 12 / 13 | 16 / 17 | port 4 idx0 / idx1 |
| copper 2 | 15 / 16 | 20 / 21 | port 5 idx0 / idx1 |
| copper 3 | 18 / 19 | 24 / 25 | port 6 idx0 / idx1 |
| copper 4 | 21 / 22 | 28 / 29 | port 7 idx0 / idx1 |
| right cage upper / lower | 9 / 11 | 12 / **14** | port 3 idx0 / **idx2** |
| left cage upper / lower | 24 / 26 | 32 / **34** | port 8 idx0 / **idx2** |

**Root cause.** Both cage lower LEDs sit on **index 2**, whose inherited
condition is `LEDS_1G | LEDS_LINK`. An LED entry only lights when the link speed
matches one of its speed bits, so `1G` never matches a 10G module and both lower
LEDs stayed dark — no mux change and no missing pad enable was involved, and
pads 11 and 26 were enabled all along.

**Fix**, applied to the target:

1. `led_sets[0][2]` becomes `LEDS_2G5 | LEDS_1G | LEDS_10G | LEDS_LINK | LEDS_ACT`.
   Index 2 is used only by those two pads — every copper LED is on index 0 or 1 —
   so the change cannot disturb the copper ports.
2. `led_mux_custom = 1` with a `led_mux[]` that points pads 9 and 24 at index 1.
   That is a behavioural match for the stock firmware, which shows a *solid*
   upper LED (link) and a *blinking* lower one (activity): index 1 has no `ACT`
   bit, so the upper LED is solid, while index 2 now has `ACT` and blinks. The
   rest of `led_mux[]` is the measured default.

### Superseded: the search for the second SFP LED

The sections below record how this was worked out and are kept for the method
(pad readback, `regset` probing, the speed-gating rule these sections describe).

### SFP LEDs: the second one is still not found

With **all four LED-set 0 entries set to `LINK | ACT` and pad 27 enabled**, every
wired LED on a linked port must blink. The second SFP LED stayed dark, so it is
**not driven by LED-set 0** — at least not through the pads assumed so far.

What that rules out:

| Ruled out | Why |
|-----------|-----|
| the condition being 0 (LED-ID 3) | all four entries were driven |
| pad 27 being disabled in `LED_GLB_IO_EN` | it was enabled first |
| the activity condition not firing | `LINK` was included, so a linked port lights regardless |

What is left, in order of likelihood:

1. **It is a 10G indicator, not a link indicator.** If the fitted module is 1G or
   2.5G, a 10G-only LED never lights. Ask what module is inserted and whether
   this LED ever lit under the stock firmware — that single answer may close it.
   A related variant: it is lit by the 10G switch, which would only take effect
   after a power cycle if the switch is a reset-time strap.
2. **Its pad is not in the port group assumed.** The LED port numbering was
derived, not measured. Identify the pads empirically instead: the LED pads are
readable through `gpio`, so sampling `gpio` repeatedly while an LED blinks shows
which bits belong to which LED. A burst of ~8 `gpio` calls with nothing touched
reveals the blinking pads directly.
3. **It is not an LED-controller output at all** — a plain GPIO the stock
firmware toggles, or a hardware line.

A brute-force check that separates 1/2 from 3: enable every pad and try again.

```
regset 65dc ffffffff    # GLB_IO_EN, all 32 pads
```

If that lights it, the pad was simply not enabled and the bit can be bisected.
If it stays dark, the LED is not on this controller's port groups.
Reboot restores everything.

Also worth re-reading `leds` after the `regset` runs, to confirm the writes
actually landed: `LED_GLB_IO_EN` should read `0x7fffff40` and SET 0 should read
`01400140` for both LED-ID0/1 and LED-ID2/3.

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
