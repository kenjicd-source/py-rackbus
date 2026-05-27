# Tested device and reference setup

## Confirmed working device

- **Manufacturer:** Endress+Hauser
- **Model:** Promass 63 (Coriolis mass flowmeter)
- **Order code:** `63FS50-FTW91A00B1B`
- **Serial number:** 484234
- **Firmware:** SW 3.00.00 / SW COM 3.00.00
- **Nominal size:** DN50
- **Communication module:** RS 485 (Rackbus SL)
- **Local display:** 3-key touch panel (E / + / −), restored from a stuck-low contrast via the "press +/− at power-on" trick documented in the operating manual

## Configuration on the device

| Matrix cell | Parameter | Value on this device |
|---|---|---|
| V2H3 | Interface | RS 485 |
| V2H4 | Bus address | **3** |
| V2H5 | System config | RS485 / current |
| V6H1 | Protocol | RACKBUS RS 485 (not OFF) |

> The access code is set to a non-default value by the previous owner.
> Reading parameters works **without** the code; writing them does not.
> All work here is read-only.

## Confirmed readable parameters

Reads obtained directly via `py-rackbus` (no gateway), matching the
local display and the ground-truth values from the publicly available
operating manual:

| V/H | Name | Type | Read value | Verified against |
|---|---|---|---|---|
| V0H0 | Mass flow            | float  | 0.0 t/h            | Local display (empty pipe) |
| V0H1 | Totalizer 1          | float  | 0.0002430857 t     | Display showed 0.0002 |
| V1H0 | Density              | float  | 0.2 g/cm³          | EPD threshold (empty pipe) |
| V1H1 | Temperature          | float  | 23.4–23.8 °C       | Room temp, rising during test |
| V2H4 | Bus address          | int    | 3                  | Configured value |
| V2H6 | SW version COM       | int    | 30000              | (matches firmware string) |
| V3H0 | Mass flow unit       | int    | 7                  | Default (t/h) |
| V7H0 | Low-flow cutoff      | float  | 0.5                | Configured |
| V7H4 | EPD threshold        | float  | 0.2                | Configured |
| V9H0 | Calibration K-factor | float  | 2.1166             | Sensor sticker |
| V9H1 | Zero point           | int    | −117               | Sensor sticker |
| V9H2 | Nominal size DN      | float  | 50.0 mm            | Order code FS50 |
| V9H5 | Serial number        | int    | 484234             | Device label |
| VAH0 | Tag                  | string | "DEMO TAG"         | Configured (anonymized for publication) |

## Test setup

| Item | Notes |
|---|---|
| USB-RS485 adapter         | ОВЕН АС4 (Silicon Labs CP2102, isolated) |
| OS / Python               | Windows 11, Python 3.13, pyserial 3.5 |
| Test environment          | Bench, no real flow (empty pipe state) |
| Bus topology              | Single master (PC) ↔ single slave (Promass) |
| Cable                     | ≈1 m twisted pair, no terminators |
| UART params               | 19200 baud, 8 data bits, no UART-parity, 1 stop bit |
| Software parity           | Even parity in MSB of every payload byte, computed in code |
| Inter-chunk timing        | 50 ms between master TX chunks |
| Per-transaction time      | ~250 ms (first attempt) to ~700 ms (with one retry) |
| Stability                 | All-day on/off cycling, no transient failures observed |

## Wiring (standard RS-485)

```
                                       ┌──────────────────────┐
USB ──── USB-RS485 ──── A ──────────────┤  term 20 (Data A)    │
                                        │                      │
                       B ──────────────┤  term 21 (Data B)    │
                                        │   Promass 63         │
                     GND ──────────────┤  term 28 (shield/⊥)  │
                                        │                      │
                                        │  term 1 / 2: 220 VAC │
                                        └──────────────────────┘
```

Standard wiring: A↔A, B↔B. Both terminations and shield-grounding are
optional on short bench runs; on long industrial runs use 120 Ω
terminators at each end and ground the shield at one end only.

## Reproducing the results

1. Install Python and `pyserial`: `pip install pyserial`.
2. Connect adapter and Promass as above; verify Promass dispatches green
   power LED on the display.
3. Run the example:
   ```
   python examples/basic_usage.py COM5 3
   ```
4. Expect output that matches the table above.
