# py-rackbus

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Python 3.8+](https://img.shields.io/badge/python-3.8+-blue.svg)](https://www.python.org/downloads/)
[![Tests](https://img.shields.io/badge/tests-57%20passing-brightgreen.svg)](#testing)

An independent Python client for the **Rackbus SL** serial protocol —
the legacy proprietary fieldbus used by some Endress+Hauser mass
flowmeters and other Commutec-family transmitters from the mid-1990s.

This is a **clean-room reverse-engineered implementation** based on
black-box observation of unencrypted traffic on the wire. No vendor
firmware was decompiled, no vendor software was modified, no
proprietary documentation was reproduced.

> **Disclaimer.** This project is not affiliated with, endorsed by, or
> sponsored by Endress+Hauser AG. *Rackbus*, *Commubox*, *Promass*,
> *Commutec* may be trademarks of their respective owners — used here
> purely descriptively. The wire-format description in this repository
> is derived solely from observed traffic on equipment lawfully owned
> by the author. See [NOTICE.md](NOTICE.md) for full legal positioning.

---

## Why this exists

Many Coriolis flowmeters and process instruments from 1995-2003 use
the Rackbus SL protocol over RS-485. The vendor's original software
(Commuwin II) was discontinued long ago, requires legacy hardware
dongles, and runs only on Windows 95/98/XP.

These instruments are still in production use across dairy plants,
breweries, chemical facilities, and pharmaceutical lines. Replacing
them costs tens of thousands of euros per unit plus weeks of
downtime.

This library lets you read live data from such instruments using
modern, cheap hardware:

- A $5 USB-to-RS485 adapter and Python — for laptops and gateway PCs
- An ESP32 + MAX485 module — for embedded telemetry, MQTT, Home Assistant
- Any platform with a serial port and an RS-485 transceiver

Read-only by design — you can pull mass flow, density, temperature,
totalizers, and tags without risk of accidentally writing wrong
configuration to live equipment.

---

## Quick start

```bash
git clone https://github.com/kenjicd-source/py-rackbus.git
cd py-rackbus
pip install pyserial
python examples/basic_usage.py COM5 3
```

That assumes the instrument is at bus address 3 on `COM5`. Replace
with whatever you have. Wiring:

```
Instrument terminal A (or +)  ─── USB-RS485 A (D+)
Instrument terminal B (or -)  ─── USB-RS485 B (D-)
Instrument shield / ⊥         ─── USB-RS485 GND  (optional)
```

Standard RS-485 polarity. See [hardware/README.md](hardware/README.md)
for diagrams and tips.

---

## API

```python
from rackbus_client import RackbusClient

with RackbusClient(port='COM5', address=3) as client:
    print(client.read_mass_flow())          # → float (t/h)
    print(client.read_density())            # → float (g/cm³)
    print(client.read_temperature())        # → float (°C)
    print(client.read_totalizer_1())        # → float (t)
    print(client.read_tag_number())         # → str
    print(client.read_parameter('V0H0'))    # generic by V/H code
    print(client.read_all())                # → dict of common params
```

Full API surface and module layout: [docs/STRUCTURE.md](docs/STRUCTURE.md).

---

## ESP32 firmware

A working PlatformIO firmware for ESP32-S2 (and any other ESP32 variant)
is included in [`firmware/`](firmware/). It reads the Promass over
RS-485 via a MAX485 module, serves a local web UI with live values, and
optionally posts JSON telemetry to a remote endpoint.

```bash
cd firmware
cp include/secrets.h.example include/secrets.h
$EDITOR include/secrets.h         # WiFi, OTA password, telemetry URL
pio run -t upload                  # first flash over USB
pio device monitor                 # see live transactions
```

After that you can flash over WiFi (`pio run -t upload -e ota`) and
open `http://promass.local/` in a browser.

See [firmware/README.md](firmware/README.md) for the full guide and
especially the **MAX485 turnaround timing** warning — that one will
save you hours.

---

## Status

| Component | Status |
|---|---|
| Frame structure (request + response, long + short) | ✅ Fully reversed |
| Address encoding (`0x5F − ASCII` + parity) | ✅ Verified |
| V/H encoding (`0x5F − raw int` + parity) | ✅ Verified |
| Data decoding (ASCII complement, scale-char, integers, strings, `@` invalid) | ✅ Verified on real values |
| Request checksum | ✅ Reversed (`crc1 = 0x2E XOR …`, `crc2 = sum + 0x48`) |
| Response checksum | ⚠ Stub (slave trusted; verifier always returns `True`) |
| Multi-stage exchange (warmup → VR → DD → 48) | ✅ Reversed and reliable |
| Test suite | ✅ 57 tests passing |
| Confirmed device | Promass 63 (`63FS50-FTW91A00B1B`, SW 3.00.00) |
| ESP32 firmware port | ✅ Working (`firmware/`, PlatformIO, MAX485) |
| Modbus TCP gateway | 🚧 Planned |

---

## Supported devices

| Manufacturer | Model | Firmware | Tested | Notes |
|---|---|---|---|---|
| Endress+Hauser | Promass 63 | SW 3.00.00 / SW COM 3.00.00 | ✅ | Reference device |

Likely compatible (untested) — please report:

- Other Endress+Hauser Promass 60-series with RACKBUS RS 485 module
- Other Commutec-family transmitters of the same generation
  (Promag 50/53, Levelflex, Cerabar) in their Rackbus variants

If you have one on the bench, please file a
[compatibility report](../../issues/new?template=compatibility_report.md).

---

## Documentation

- [docs/PROTOCOL.md](docs/PROTOCOL.md) — Wire protocol specification
  (frame format, encoding, checksum, multi-stage exchange)
- [docs/PARAMETERS.md](docs/PARAMETERS.md) — V/H parameter map for
  supported instruments
- [docs/EXAMPLE_TRANSACTIONS.md](docs/EXAMPLE_TRANSACTIONS.md) —
  Byte-for-byte capture examples with decoded values
- [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md) —
  How the protocol was figured out, dead ends, lessons learned
- [docs/STRUCTURE.md](docs/STRUCTURE.md) — Code layout and API surface
- [hardware/README.md](hardware/README.md) — Wiring diagrams for USB
  adapters and ESP32
- [firmware/README.md](firmware/README.md) — ESP32-S2 firmware
  (PlatformIO), HTTP/JSON API, telemetry publisher, OTA updates,
  and a sharp warning about MAX485 turnaround timing

---

## Testing

```bash
pip install pytest pyserial
pytest
```

Expected output: `57 passed`.

The test suite uses byte-for-byte fixtures captured from a real device
and runs offline (no hardware needed for tests).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Particularly welcome:

- Compatibility reports for other Rackbus devices
- Response-CRC reverse engineering (we have the captured material; the
  formula isn't found yet)
- ESP32 / Arduino / STM32 ports
- Wireshark dissector
- Modbus TCP gateway

---

## License

MIT — see [LICENSE](LICENSE). All code in this repository is original
work by the contributors and is not derived from any proprietary
source.

For the legal positioning of the reverse-engineering effort itself
(EU Software Directive 2009/24/EC, US 17 USC § 1201(f), trademark
notes), see [NOTICE.md](NOTICE.md).

---

## Safety notice

This library is intentionally **read-only**. Write operations to live
industrial process instruments can cause equipment damage, process
upsets, or safety incidents. Always test on a bench setup before
connecting to a production line.
