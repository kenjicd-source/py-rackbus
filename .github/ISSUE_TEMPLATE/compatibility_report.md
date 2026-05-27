---
name: Compatibility report
about: Report success or failure with a specific instrument
title: '[COMPAT] '
labels: compatibility
assignees: ''
---

## Instrument

- **Manufacturer:**
- **Model:**
- **Full order code:**
- **Firmware version:**
- **Communication module:** RS 485 (Rackbus) / HART / other

## Result

- [ ] Fully working — all expected parameters read correctly
- [ ] Partially working — see notes below
- [ ] Not working — see notes below

## What worked

Which parameters did you successfully read? Did values match the
instrument's local display?

| Parameter | V/H | Read value | Display value | Match? |
|---|---|---|---|---|
| Mass flow | V0H0 |           |              |        |
| Density   | V1H0 |           |              |        |
| ...       |     |            |              |        |

## What didn't work

If something failed, describe what happened.

## Hardware setup

- USB-RS485 adapter model:
- ESP32 / other board (if used):
- Baud rate used:
- Bus address of instrument:

## Notes

Anything else? Device-specific quirks? Different parameter map?
