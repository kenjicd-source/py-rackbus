# ESP32 firmware — Promass 63 reader over Rackbus SL

Reads parameters from an Endress+Hauser Promass 63 (or compatible
Rackbus SL slave) directly via RS-485, exposes an HTTP/JSON API for a
local UI, and optionally pushes telemetry to a remote ingest endpoint
(any HTTPS server that accepts POST + API key).

## Hardware

| Item | Notes |
|---|---|
| **ESP32-S2 mini** (Wemos LOLIN) | Default target. Other ESP32 work too — change env in `platformio.ini`. |
| MAX485 / MAX3485 module       | Cheap blue/red board with `RO / DI / DE / RE / A / B / VCC / GND`. |
| Three short wires             | A, B, GND to the Promass field-terminal block. |
| 5 V (or 3.3 V) source         | The ESP32 USB rail powers everything during the bench tests. |

## Wiring

```
ESP32-S2 (Wemos LOLIN)             MAX485 module                    Promass 63
─────────────────────              ─────────────                    ──────────
GPIO 35 (TX) ───────────────────►  DI
GPIO 33 (RX) ◄───────────────────  RO
GPIO 37      ──────────┬────────►  DE  ┐ jumper / one wire
                       └────────►  RE  ┘ to both pins
5V           ───────────────────►  VCC
GND          ───────────────────►  GND
                                   A   ─────► term 20 (Data A)
                                   B   ─────► term 21 (Data B)
                                   GND ─────► term 28 (shield)
```

> **No bias resistors required** in our setup — Promass + ESP32 +
> MAX485 idle level is stable enough on a short bench cable. On longer
> cable runs add 680 Ω from `A → VCC` and 680 Ω from `B → GND`. Add
> a 120 Ω terminator between A and B at the far end on cables >5 m.

## ⚠️ MAX485 turnaround timing — the trap

USB-RS485 dongles (CP2102, CH340) hide this from you. Discrete MAX485
exposes it. The danger: after `Serial.write(...)` + `Serial.flush()` the
**last byte may still be on the wire**, because flush() on some Arduino
cores only waits for the software buffer, not the hardware FIFO. If you
release `DE` too early — your last bit is clipped. If you release it too
late — the slave starts answering before you let the bus go, causing a
collision and producing garbage bytes (we saw `26`, `93`, doubled `F2`).

What we do in this firmware (`src/rackbus_client.h::tx()`):

```cpp
digitalWrite(de_re, HIGH);
delayMicroseconds(100);      // brief settle
ser->write(buf, n);
ser->flush();                // ESP32-S2 Arduino: blocks until HW TX done
digitalWrite(de_re, LOW);    // release bus immediately
delayMicroseconds(100);      // receiver settle
```

If you port to a different chip / framework where `flush()` does **not**
wait for hardware completion, add `delayMicroseconds(11_000_000 / baud)`
between `flush()` and `digitalWrite(LOW)` — that's exactly one byte time.

The truly bulletproof solution is ESP-IDF's hardware mode:

```cpp
uart_set_mode(UART_NUM_X, UART_MODE_RS485_HALF_DUPLEX);
uart_set_pin(UART_NUM_X, TX_PIN, RX_PIN, DE_RE_PIN, UART_PIN_NO_CHANGE);
```

— ESP32 hardware then toggles DE on the exact bit boundary. A migration
to this mode is planned (see TODO at the bottom).

## Build / flash

### 1. Secrets

```bash
cp include/secrets.h.example include/secrets.h
$EDITOR include/secrets.h         # set WiFi, OTA password, telemetry URL + key
```

`include/secrets.h` is in `.gitignore` — don't commit it.

### 2. First flash via USB

```bash
pio run -e lolin_s2_mini -t upload
pio device monitor                  # 115200 baud, watch for the IP
```

For the Wemos LOLIN S2 Mini you may need to enter the bootloader
manually the first time: hold `BOOT`, tap `RST`, release `BOOT`. The
board will appear as a USB-CDC COM port; PlatformIO uploads to it.

### 3. Subsequent updates via WiFi (OTA)

The OTA password is read from the `OTA_PASSWORD` environment variable
(same value as `OTA_PASSWORD` in `secrets.h`):

```bash
# Windows (PowerShell):  $env:OTA_PASSWORD = "your-ota-password"
# Linux / macOS:         export OTA_PASSWORD=your-ota-password

pio run -e lolin_s2_mini_ota -t upload    # uses mDNS promass.local
```

No USB cable needed once the firmware is running on the device. Make
sure your dev machine is on the same Wi-Fi network or has a route to
the device.

## Endpoints

Once running, the device serves:

| URL | What |
|---|---|
| `http://<ip>/` | HTML status page with auto-refresh |
| `http://<ip>/api/values` | JSON: all parameters + RSSI + counters |
| `http://<ip>/api/raw?v=0&h=0` | Read an arbitrary V/H once |
| `http://<ip>/api/sniff?ms=3000` | Passively sniff the bus for N ms |
| `http://<ip>/api/last_publish` | Last upload to telemetry endpoint (HTTP code, body) |
| `http://<ip>/api/polling/{on,off}` | Toggle the VR polling loop |
| `http://<ip>/api/publish/{on,off}` | Toggle the telemetry uploader |
| `http://<ip>/health` | `{"status":"ok","polling":...}` |

The device also advertises itself via mDNS as `<MDNS_NAME>.local`
(default `promass.local`).

## What's in here

```
firmware/
├── platformio.ini             default + ESP32-S2 + OTA envs
├── README.md                  this file
├── .gitignore
├── include/
│   ├── secrets.h.example      template — copy and fill
│   └── secrets.h              (gitignored) real WiFi / API key
└── src/
    ├── rackbus_codec.h        encoder/decoder (port of codec.py)
    ├── rackbus_client.h       high-level client (warmup + exchange + retry)
    └── main.cpp               polling loop, HTTP server, telemetry publisher
```

## TODO (roadmap)

- Migrate `rackbus_client::tx()` to ESP-IDF `UART_MODE_RS485_HALF_DUPLEX`
  so DE switching is fully hardware-managed.
- NTP time sync + absolute UTC timestamps in published samples
  (currently the server stamps on receive).
- Optional buffered batch upload (`POST` an array of samples) when the
  connection is intermittent — falls back to single-sample as today
  when the link is up.
- Plato conversion endpoint (`/api/wort`) — applies temp-compensation
  on density and returns °P / °Bx / SG for live wort monitoring.

## Provenance / license

The Rackbus SL frame format used in `rackbus_codec.h` was reverse-engi-
neered from a stock Endress+Hauser ZA672 gateway driving a real
Promass 63 (a unit owned by the project author). No vendor source code,
binaries or documentation are reproduced.

License: MIT (see `LICENSE` at repo root).
