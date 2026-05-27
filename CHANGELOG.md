# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-05-26

Initial public release.

### Added

- Complete Rackbus SL protocol implementation:
  - Frame encoding/decoding (request and response forms)
  - Address encoding (`0x5F - ASCII` + parity)
  - V/H matrix encoding (`0x5F - raw_int` + parity)
  - Data decoding for integers, floats (with implicit decimal point),
    strings, and the `@` invalid-value sentinel
  - Multi-stage exchange pattern (warmup polls → VR → CONT_DD → CONT_48
    → listen, with optional retries on empty responses)
- Request CRC formula (reverse-engineered, verified on 110 transactions):
  - `crc1 = 0x2E XOR (0x08 if odd-MSB-count else 0)`
  - `crc2 = (sum_low7 + 0x48) mod 256`
- High-level `RackbusClient` with convenience methods:
  `read_mass_flow()`, `read_density()`, `read_temperature()`,
  `read_totalizer_1/2()`, `read_volume_flow()`, `read_tag_number()`,
  `read_bus_address()`, `read_calibration_factor()`,
  `read_serial_number()`, `read_parameter()`, `read_all()`
- V/H parameter map for Endress+Hauser Promass 63
- 57-test regression suite with byte-for-byte fixtures
- **ESP32 firmware** (`firmware/`, PlatformIO):
  - Port of `codec.py` / `client.py` to C++ headers
    (`rackbus_codec.h`, `rackbus_client.h`)
  - WiFi + mDNS + HTTP/JSON API + small live web UI
  - Optional telemetry publisher (POST JSON to any HTTPS endpoint)
  - Over-the-air firmware updates (ArduinoOTA)
  - `secrets.h.example` template + `.gitignore` to keep credentials
    out of the repository
  - Documented warning about MAX485 turnaround timing — the trap
    every embedded RS-485 implementer hits when moving from a USB
    dongle to discrete MAX485
- Documentation:
  - Wire protocol specification (`docs/PROTOCOL.md`)
  - V/H parameter map (`docs/PARAMETERS.md`)
  - 10 captured transaction examples (`docs/EXAMPLE_TRANSACTIONS.md`)
  - Reverse-engineering writeup (`docs/REVERSE_ENGINEERING.md`)
  - Hardware wiring guide (`hardware/README.md`)
  - ESP32 firmware guide (`firmware/README.md`)

### Known issues / open work

- Response CRC formula not yet fully reverse-engineered. The current
  implementation does not verify response checksums. Reverse-engineering
  material is available in the captured transaction set.
- Semantics of the `CA DD AA 28 F2` continuation frame are unknown
  empirically — it is mandatory but its internal meaning has not been
  decoded.
- Variable Write (VW) not implemented (read-only by design).
