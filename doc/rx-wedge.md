# The parked RX buffer

Rapid HTTP connection churn could wedge the management plane for good on some
boards: the switch stops answering ICMP, the web UI and the console, the ASIC
keeps forwarding, and only a power cycle brings it back. This is the write-up of
one such case, on a PCB-K0402WS-V2.0 (RTL8372), and of the fix that went with it.

## Symptom

- ICMP, the web UI and the serial console all go quiet at the same time.
- The ASIC keeps switching: clients behind the box stay connected and reachable.
- Nothing recovers it but a power cycle.

## Trigger

Roughly a dozen back-to-back HTTP connections:

```
for i in $(seq 1 30); do curl -s -o /dev/null http://192.168.10.247/login.html; done
```

The switch wedged on request 13. Requests a few seconds apart did not reproduce
it. A plain GET loop is enough, so the fault is in the receive path and not in
any one endpoint.

## Ruled out

The first readings of the symptom pointed at the wrong things. The evidence that
cleared each of them is worth keeping, so the next run does not repeat the
detour.

- **The login handler, and POST in general.** A wrong-password `POST /login`
  never reaches the password check or the session id, and a `POST /foo` that
  ends in a 404 did not wedge at all. What did wedge was a plain loop of
  `GET /login.html`, so no endpoint is special.
- **The session id's random source** (`get_random_32()`, which reads the RLDP
  registers at 0x106c/0x107c). It runs only on a *successful* login, and
  wrong-password logins wedged the switch just as well.
- **The empty SFP cage.** `/information.json` walks both EEPROMs whether a
  module is present or not, which is why the wedge first looked tied to the page
  after login, but the reproduction above gets there without any authenticated
  request. (Not reading an empty cage is still a good fix, it just is not this
  one.)
- **The web server's connection handling.** `make -C test` drives twenty
  sequential connections through the unmodified `httpd.c` under ASan and every
  one is served cleanly, so the state machine is not what dies.
- **"It recovers after about half a minute."** It does not. The recoveries seen
  during the investigation were manual power cycles; left alone the switch stays
  down, which is what makes the tick poll and the stall watchdog necessary rather
  than cosmetic.

## Why

The main loop drained the NIC receive buffer only while `rx_irq` was set, and
`rx_irq` comes from the NIC's RX interrupt. Once the buffer fills, the NIC stops
raising that interrupt, so nothing empties it again. The re-check #517 added
lives inside the same `if (rx_irq)` guard and therefore never runs either.
Management dies with the buffer; the switch keeps forwarding because that path
never touches the CPU.

## Fix

- `handle_tick()` reads `RTL837X_REG_NIC_RX_BUFF_DATA` once per system tick and
  drains whatever it holds, so the buffer cannot park regardless of interrupts.
- If the fill level stays non-empty for `RX_STALL_TICKS` (5 s), the NIC is reset
  on its own (`RESET_NIC_BIT` then `nic_setup()`), which is cheaper than a chip
  reset and does not lose the rest of the state.
- A main-loop deadman resets the chip when `idle()` has not run for ten seconds,
  for the case where the loop itself stops.

## Verified

A Ganwen GW-9000-6XH-X2 ran this fix for 7.3 days without the wedge coming
back. The diagnostic readout that has since been removed reported:

```
{"ticks":"0x077f987c","loops":"0xd7395063","rx_poll":0,"rx_resets":0,
 "nic_sts":"0x02","nic_buf":"0x0000"}
```

`ticks` is 7.3 days at 200 Hz, `nic_buf` is empty and `rx_poll`/`rx_resets`
are both zero: the tick poll never had to catch a parked buffer and the NIC
was never reset.

## Still open

`loops` climbs much faster than `ticks`, so the main loop spins instead of
idling at `PCON |= 1` — the 8051 IDLE bit does not appear to take effect on
this part. It does not affect the fix, but the CPU runs at full tilt and so
do the power and heat that go with it. The exact ratio cannot be derived from
a single sample, because `loops` is a 32-bit counter that wraps: all that is
known is `loops = rate * ticks (mod 2^32)`.
