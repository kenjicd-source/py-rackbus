# Hardware

How to physically connect your hardware to the instrument.

## Cable

- Shielded twisted pair, 22-24 AWG
- Maximum total bus length: ~1200 m at 9600 baud, less at higher
  speeds (we run at 19200)
- Connect the shield to ground at **one end only** (typically the
  master end) to avoid ground loops

## Termination

For short bench setups (under a few meters), termination is not
strictly necessary. For longer runs:

- 120 Ω resistor between A and B at the **far end** of the bus
- 120 Ω at the master end as well (most USB-RS485 adapters have this
  built-in or selectable via DIP switch)

## Connecting to the instrument

The instrument terminal block typically has labels like `RS 485 A`,
`RS 485 B`, and `⊥` (shield). On the reference device used for
development of this library (Endress+Hauser Promass 63), these are
terminals 20, 21, and 28 respectively.

Refer to your instrument's operating manual for exact terminal
assignments.

## USB-to-RS485 adapter (for PC use)

Any RS-485 adapter works in principle. Confirmed working:

- **Generic CH340 / CP2102-based modules** — cheap, good enough for
  development
- **Isolated industrial adapters** (e.g. OWEN AC4, Moxa USB) — recommended
  for any real installation near 220V mains or in noisy industrial
  environments
- **FTDI FT232-based adapters** — most stable across operating systems

Wiring (standard RS-485 polarity):

```
Instrument terminal A (Data A / D+)  ─── Adapter A  (D+)
Instrument terminal B (Data B / D-)  ─── Adapter B  (D-)
Instrument shield / ⊥                ─── Adapter GND (optional)
```

If you don't get a response, double-check that A is going to A and B
to B. The protocol does not invert polarity — standard RS-485 wiring
applies.

## ESP32 wiring (for embedded use)

Required components:
- ESP32 dev board (DevKit C, S2, S3 — any will work)
- MAX485 module (or equivalent: SP3485, isolated ADM2587)
- Optional: power supply

Pin assignments (configurable in firmware):

```
ESP32 GPIO16 (UART2 RX) ─── MAX485 RO
ESP32 GPIO17 (UART2 TX) ─── MAX485 DI
ESP32 GPIO 4 (any)      ─── MAX485 DE + RE (tied together)
ESP32 3V3               ─── MAX485 VCC
ESP32 GND               ─── MAX485 GND

MAX485 A  ─── Instrument terminal A
MAX485 B  ─── Instrument terminal B
MAX485 GND (optional) ─── Instrument shield
```

GPIO 4 (or any free GPIO) controls the MAX485 direction:
- HIGH = transmit mode
- LOW = receive mode

The firmware handles direction switching automatically based on
transmit/receive operations.

## Power isolation

For development on a bench, powering everything from the laptop's USB
is fine. For installation in industrial environments:

- Use an **isolated** USB-RS485 adapter or **isolated** RS-485
  transceiver (ADM2587, ISL3179, etc.)
- Never connect the instrument's signal ground directly to your PC's
  ground if the instrument is powered from mains — ground-loop
  currents can damage equipment and create safety hazards

## Sanity check before connecting

1. Verify the instrument is powered and operational (check its local
   display)
2. Verify your adapter is recognized by the OS (appears as a serial
   port)
3. Verify wiring polarity (A to A, B to B)
4. Start with passive listening (no transmission) for a few seconds to
   confirm electrical sanity — you should see silence, not noise

If anything seems wrong, **stop and check** before sending any
frames. A wiring mistake on RS-485 won't usually damage equipment,
but it can cause communication failures that look like protocol
issues.
