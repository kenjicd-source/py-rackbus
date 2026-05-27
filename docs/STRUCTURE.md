# Code structure

```
py-rackbus/
├── rackbus_client/         ← The Python package
│   ├── __init__.py         ← public API exports
│   ├── exceptions.py       ← RackbusError, RackbusTimeoutError, …
│   ├── codec.py            ← frame encoding/decoding (parity, complement, split)
│   ├── crc.py              ← request CRC formula (response CRC stub)
│   ├── parameters.py       ← V/H map of Promass 63 parameters (VHCode dataclass)
│   ├── transport.py        ← serial port + multi-stage exchange pattern
│   └── client.py           ← high-level RackbusClient + convenience methods
├── tests/                  ← Test suite (offline, no hardware needed)
│   ├── conftest.py
│   ├── test_protocol.py    ← 57 regression tests
│   └── test_fixtures.json  ← byte-for-byte fixtures
├── examples/
│   └── basic_usage.py      ← runnable demo (needs hardware)
├── firmware/               ← ESP32-S2 PlatformIO project
│   ├── platformio.ini
│   ├── include/
│   │   └── secrets.h.example
│   ├── src/
│   │   ├── rackbus_codec.h    ← C++ port of codec.py
│   │   ├── rackbus_client.h   ← C++ port of client.py
│   │   └── main.cpp           ← WiFi + HTTP API + telemetry publisher
│   └── README.md
├── docs/                   ← Documentation
│   ├── PROTOCOL.md         ← Wire protocol specification
│   ├── PARAMETERS.md       ← V/H parameter map
│   ├── EXAMPLE_TRANSACTIONS.md
│   ├── REVERSE_ENGINEERING.md
│   ├── STRUCTURE.md        ← this file
│   └── TESTED_DEVICE.md
└── hardware/
    └── README.md           ← Wiring diagrams
```

## Public API surface

Imported from `rackbus_client`:

| Symbol | Where it lives | Purpose |
|---|---|---|
| `RackbusClient`         | `client.py`     | High-level client; one instance per port |
| `RackbusError` (base)   | `exceptions.py` | Common base for all client errors |
| `RackbusTimeoutError`   | `exceptions.py` | Slave gave only empty / no responses |
| `RackbusChecksumError`  | `exceptions.py` | Reserved (response CRC not yet checked) |
| `RackbusDecodeError`    | `exceptions.py` | Frame structurally unparseable |
| `PARAMETERS` (list)     | `parameters.py` | Full V/H map of known parameters |
| `VHCode` (dataclass)    | `parameters.py` | Single-parameter description |

Lower-level helpers are available via `rackbus_client.codec`,
`rackbus_client.crc`, `rackbus_client.transport` for users who want to
build their own client on top of the encoder/decoder.

## Convenience methods on `RackbusClient`

```python
client.read_mass_flow()           # → float (t/h)
client.read_density()             # → float (g/cm³)
client.read_temperature()         # → float (°C)
client.read_totalizer_1()         # → float (t)
client.read_totalizer_2()         # → float (t)
client.read_totalizer             # alias of read_totalizer_1
client.read_volume_flow()         # → float (m³/h)
client.read_tag_number()          # → str
client.read_bus_address()         # → int (sanity check)
client.read_calibration_factor()  # → float
client.read_serial_number()       # → int

client.read_parameter('MASS_FLOW')   # generic by name
client.read_parameter('V0H0')        # or by V/H code

client.read_all()                 # → dict of all common parameters
client.read_all(['MASS_FLOW',     # custom subset
                 'DENSITY'])

client.close()                    # always close to release the port
```

A `with RackbusClient(...) as c:` context manager closes
automatically.

## Module dependency graph

```
client.py
  └── transport.py
       ├── codec.py
       │    └── crc.py
       ├── exceptions.py
       └── parameters.py (via client.py)
```

Only `transport.py` and `client.py` import `pyserial`. Pure protocol
code (`codec.py`, `crc.py`, `parameters.py`, `exceptions.py`) is
dependency-free and runs offline — useful for unit tests, codegen, or
running on exotic environments.
