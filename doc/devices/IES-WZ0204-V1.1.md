IES-WZ0204-V1.1
================

RTL8372-based industrial 4×2.5G + 2×SFP+ unmanaged switch. White-label, with no
retail brand found for the model number.

Also carries two slide switches, one of which is used by this target, see
*Mode switch* below.

### Label specifications

- **Ports**:
  - 4 × RJ45: 10/100/1000/2500 Mbps
  - 2 × SFP+: 1G / 2.5G / 10G
- **PCB marking**: `IES-WZ0204-V1.1`

### What works

- All four 2.5GBASE-T RJ45 ports
- The left SFP+ cage **at 10G and at 1G**, with its I²C/DDM working. Verified with a
  Finisar `FCBG110SD1C05-1RA` (a 10G SFP+ active optical cable) and with an ETU
  `ESB3512-3SCD20` 1G module, in both cases with the link up. `regget 7b20`
  confirms the rate comes from the module's EEPROM: bits 5-9 (SDS 1, the left
  cage) read `0x04`, i.e. 1000BX_FIBER, with the 1G module fitted.
- Copper LEDs and both cage LEDs
- The VLAN mode switch, at boot and while running
- Management over the web UI at 115200-independent `192.168.10.247`

### What has not been verified

- The right SFP+ cage: its `pin_detect`, `pin_los`, I²C bus and SerDes have
  never been exercised, no module has been fitted to it.
- Copper 1 and 4 of the port order. Copper 2 and 3 were confirmed by watching
  which LEDs move when the management cable is moved between them.

### PCB overview

**Key components**
- Main switch: RTL8372 (RTL8371B family), reported as `RTL8372` (non-`N`) by
  RTLPlayground, confirmed by reading the chip id register (`regget 0004` reads
  `0x83720000`)
- SPI NOR flash: Boya/BoHong `BY25D40`, JEDEC `0x68 0x40 0x13`, 4 Mbit / 512 KB,
  SOP-8. RTLPlayground's 512 KB image fills it exactly, so there is no room for
  the web update image.
- Two slide switches on the case, labelled VLAN and 10G

### Flash: single-IO reads are required

`BY25D40` offers `0x03`, `0x0B` and `0x3B` (dual *output*) but no `0xBB` dual-I/O
read, so the SoC cannot fetch code with the DIO read command. The target
therefore sets `.flash_sio = 1`, which selects `0x0B`. This is not a
precaution — DIO does not work on this chip at all.

### Serial console

**None is exposed.** There are no console through-holes on this board, so the
stock firmware's 9600 baud 8N1 factory test console cannot be reached and the
boot log cannot be watched. Everything is done over the web UI.

### Port layout

| Front panel position | Logical port | Physical port | Type    |
|----------------------|--------------|---------------|---------|
| SFP left             | 8            | 5             | SFP+    |
| RJ45 1               | 4            | 1             | Copper  |
| RJ45 2               | 5            | 2             | Copper  |
| RJ45 3               | 6            | 3             | Copper  |
| RJ45 4               | 7            | 4             | Copper  |
| SFP right            | 3            | 6             | SFP+    |

Copper ports 1–4 match the GW-9000-6XH-X2 V1.1 layout, and the two SFP+ cages are
wired the other way round compared to that board: physical port 5 is on SDS1 and
physical port 6 on SDS0.

Which cage is which is confirmed independently of this table: each cage's lower
LED is wired to the pad whose LED-ID names that port — pad 26 = port 8 for the
left cage, pad 11 = port 3 for the right one, see the pad map under *LEDs*. A
module in the left cage also reports as slot 1, which is `sfp_port[0]`, the one
this table calls logical 8.

Of the copper ports only 2 and 3 have been confirmed, by watching which LEDs move
when the management cable is moved between them.

### LEDs

Each copper port has two LEDs and **each cage has two as well**, where the
GW-9000 and its relatives have a single LED per cage.

The ASIC's default pad→LED-ID mux already matches the board — every LED's pad is
assigned to its own port — so the copper ports need no mux changes. The cages'
lower LEDs, however, sit on LED-index 2, and an LED entry only lights when the
link speed matches one of its speed bits. The inherited index 2 entry,
`LEDS_1G | LEDS_LINK`, can therefore never match a 10G link, which is why both
lower LEDs stayed dark with a 10G module fitted.

The target changes two things:

1. `led_sets[0][2]` gains `2G5`, `10G`, `LINK` and `ACT`. Index 2 is used only by
   those two pads — every copper LED is on index 0 or 1 — so the copper ports are
   unaffected.
2. `led_mux_custom` points the two upper cage pads (9 and 24) at index 1, which
   has no `ACT` bit. That gives a solid upper LED and a blinking lower one, which
   is what the stock firmware shows.

Measured pad map, for reference:

| LED | Pad | (port, index) |
|-----|-----|---------------|
| copper 1 left / right | 12 / 13 | port 4 idx0 / idx1 |
| copper 2 | 15 / 16 | port 5 idx0 / idx1 |
| copper 3 | 18 / 19 | port 6 idx0 / idx1 |
| copper 4 | 21 / 22 | port 7 idx0 / idx1 |
| right cage upper / lower | 9 / 11 | port 3 idx0 / idx2 |
| left cage upper / lower | 24 / 26 | port 8 idx0 / idx2 |

As on the GW-9000-6XH-X2, `RTL837X_REG_LED_GLB_IO_EN` bit 6 has to be set, and
that is done for this machine in `machine_custom_init()`.

### Mode switch

The case has a **VLAN** switch and a **10G** switch. Neither has an ON/OFF
marking; both were shipped in the position that made the unit behave as a plain
unmanaged switch.

**VLAN switch: GPIO31**, active low — closed to ground is the engaged position.
When engaged, the **copper ports are isolated from each other** while every one of
them can still reach both cages, and the cages can reach every copper port.

Note the direction, because it is easy to get backwards: it is copper against
copper with the cages as the shared uplink, *not* copper against cages.

The switch sits on the UART0 TX pin, which this board does not bring out, so the
target gives up the UART function to read it as a plain GPIO. That also removes a
shorting concern: a closed switch would otherwise pull the UART output to ground
on every byte transmitted.

There is a second pin involved, unrelated to the switch: `GPIO54` reads low
permanently, as it does on the boards above, so `reset_pin` stays `GPIO_NA`. The
board has no reset button.

**10G switch: not a GPIO, and no effect under RTLPlayground.** It was flipped
repeatedly with the SFP cages empty and no bit in the GPIO input registers ever
moved, so it is not a readable GPIO.

What it actually does was settled with a **1G** module, the only rate that can
tell "force 10G" apart from "adaptive": `regget 7b20`
(`RTL837X_REG_SDS_MODES`, bits 0-4 = SDS 0, bits 5-9 = SDS 1) reads `0x9f`, i.e.
SDS 1 = `0x04` = 1000BX_FIBER, matching the 1G module. It reads exactly the same
with the switch flipped while running, and after power cycles with the switch in
each position. A forced-10G strap would have shown `0x1a` (10GR), or a link that
never comes up with a 1G module. It does neither.

The reason is structural: the firmware configures the SerDes from the module's
EEPROM rate (`sfp.c`), so a hardware strap would be overwritten in any case. The
switch is therefore deliberately not implemented.

### Machine target

```
MACHINE_IES_WZ0204_V1_1
```

Settings beyond the usual: `flash_sio = 1` (see above), `pin_mode_vlan =
GPIO31_UART0_TX` (see above), and the LED changes above. The SFP `pin_detect`,
`pin_los` and I²C settings are inherited from the sibling boards; of those, only
the left cage's have been verified.
