# Parameter map (V/H matrix)

The instrument exposes its measurements and configuration through a
two-dimensional matrix indexed by V (vertical, 0-A) and H (horizontal,
0-9). This document maps coordinates to human-readable parameter
names.

## Common parameters (read-only via this library)

### V0 — Process variables

| V/H | Name | Type | Default unit |
|-----|------|------|--------------|
| V0H0 | Mass flow | float | t/h |
| V0H1 | Totalizer 1 | float | t |
| V0H2 | Totalizer 1 overflow counter | int | count |
| V0H3 | Totalizer 2 | float | t |
| V0H4 | Totalizer 2 overflow counter | int | count |
| V0H5 | Volume flow | float | m³/h |
| V0H6 | Standard volume flow | float | m³/h |

### V1 — Secondary measurements

| V/H | Name | Type | Default unit |
|-----|------|------|--------------|
| V1H0 | Density | float | g/cm³ |
| V1H1 | Process temperature | float | °C |
| V1H2 | Calculated density | float | g/cm³ |

### V2 — Communication / system

| V/H | Name | Type |
|-----|------|------|
| V2H0 | Evaluation mode | int (0 = matrix 1, 1 = matrix 2) |
| V2H1 | Access code | int |
| V2H2 | Active diagnostic code | int |
| V2H3 | Interface selector | int |
| V2H4 | Bus address | int (0..63) |
| V2H5 | System configuration | int |
| V2H6 | Communication module SW version | int |

### V3 — Units

| V/H | Name |
|-----|------|
| V3H0 | Mass flow unit |
| V3H1 | Mass unit |
| V3H2 | Flowrate units |
| V3H3 | Volume units |
| V3H4 | Gallon/barrel selector |
| V3H7 | Pipe size unit |

### V4 — Totalizer & display configuration

| V/H | Name | Notes |
|-----|------|-------|
| V4H0 | Reset totalizer | 0:cancel, 1:rst1, 2:rst2, 3:both |
| V4H1 | Variable summed by totalizer 1 | enum |
| V4H2 | Variable summed by totalizer 2 | enum |
| V4H3 | LCD contrast | int |
| V4H4 | Display language | int |
| V4H5 | Display damping | int |
| V4H6 | Display line 1 content | enum |
| V4H7 | Display line 2 content | enum |
| V4H8 | Flow format | int |

### V5 — Current output (4-20 mA)

| V/H | Name |
|-----|------|
| V5H0 | Output variable assignment |
| V5H1 | Value at 0 / 4 mA |
| V5H2 | Full-scale value |

### V6 — Pulse / frequency output

| V/H | Name |
|-----|------|
| V6H0 | Pulse/frequency variable |
| V6H3 | Pulse width |

### V7 — Process configuration

| V/H | Name | Default unit |
|-----|------|--------------|
| V7H0 | Low-flow cutoff | t/h |
| V7H1 | EPD function | enum |
| V7H4 | Empty pipe detection threshold | g/cm³ |
| V7H5 | EPD response code | int |
| V7H6 | EPD alarm enable | bool |

### V8 — Amplifier

| V/H | Name |
|-----|------|
| V8H5 | Amplifier software version |

### V9 — Sensor / calibration

| V/H | Name | Notes |
|-----|------|-------|
| V9H0 | Calibration factor (K-factor) | float |
| V9H1 | Zero point | int (often negative) |
| V9H2 | Pipe nominal size | mm |
| V9H5 | Sensor serial number | int |

### V10 (= VA) — Identification

| V/H | Name | Type |
|-----|------|------|
| V10H0 | User-assigned tag | string |

---

## Device-specific notes

Parameter availability depends on instrument model and firmware
version. The coordinates above were verified on a Promass 63 with
firmware SW 3.00.00 / SW COM 3.00.00.

Other instruments using the Rackbus SL protocol may have similar but
non-identical parameter layouts. When in doubt:

- Compare values returned via the protocol with values shown on the
  instrument's local display
- Reference the instrument's publicly available operating manual for
  parameter semantics

## Adding a new device

If you have successfully tested this library with another device,
please open a PR adding its parameter map to this document. Include:

- Device model and full order code
- Firmware version
- Confirmed V/H mappings (with display ground truth)
- Any device-specific quirks
