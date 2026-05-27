// High-level Rackbus SL client for ESP32.
//
// Wraps:
//   - HardwareSerial (UART2 by default) talking to a MAX485 module
//   - DE/RE pin tied together (half-duplex direction control)
//   - Multi-stage exchange pattern: warmup → VR → CONT_DD → CONT_48 → listen
//   - Response parsing into typed value
//
// Usage:
//   RackbusClient rb(&Serial2, /*de_re_pin=*/4, /*addr=*/3);
//   rb.begin();
//   RackbusResult r;
//   if (rb.read_variable(0, 0, r)) {
//       Serial.printf("mass flow = %.4f\n", r.value_f);
//   }

#pragma once
#include <Arduino.h>
#include "rackbus_codec.h"

struct RackbusResult {
    char kind;            // 'i' int, 'f' float, 's' string, 'n' invalid
    long   value_i;
    double value_f;
    char   text[24];      // for kind='s' or copy of decoded payload
    char   decoded[24];   // raw decoded string (e.g. "00!", "23.59\"", "DEMO TAG")
};

class RackbusClient {
public:
    RackbusClient(HardwareSerial *port, int de_re_pin, uint8_t address)
      : ser(port), de_re(de_re_pin), addr(address) {}

    void begin(int rx_pin = -1, int tx_pin = -1, int baud = 19200) {
        pinMode(de_re, OUTPUT);
        digitalWrite(de_re, LOW);                  // RX mode (passive)
        if (rx_pin >= 0 && tx_pin >= 0) {
            ser->begin(baud, SERIAL_8N1, rx_pin, tx_pin);
        } else {
            ser->begin(baud, SERIAL_8N1);
        }
    }

    // Returns true on success and fills `out`.
    bool read_variable(uint8_t v, uint8_t h, RackbusResult &out) {
        for (int attempt = 0; attempt < retries; attempt++) {
            warmup();
            if (do_exchange_and_parse(v, h, out)) return true;
        }
        return false;
    }

    // Tuning knobs — matched to AC4 Python client measured timing
    // (which Promass demonstrably accepts).
    int retries          = 2;
    uint32_t inter_chunk_ms = 50;    // VR → CONT_DD → CONT_48 = 50ms each
    uint32_t listen_ms      = 500;
    int warmup_polls     = 5;        // 5 presence polls
    uint32_t warmup_gap_ms = 110;    // POLL → POLL ~110ms (matches AC4)
    uint32_t after_warmup_ms = 300;  // last POLL → VR ~300ms

private:
    HardwareSerial *ser;
    int de_re;
    uint8_t addr;

    // Continuation frames captured from ZA672 sniff
    const uint8_t POLL[3]    = {0xCA, 0x3F, 0xF2};
    const uint8_t CONT_DD[5] = {0xCA, 0xDD, 0xAA, 0x28, 0xF2};
    const uint8_t CONT_48[3] = {0xCA, 0x48, 0xF2};

    // --- TX with auto direction control ---
    void tx(const uint8_t *buf, size_t n) {
        digitalWrite(de_re, HIGH);                 // driver ON
        delayMicroseconds(100);                    // brief settle
        ser->write(buf, n);
        ser->flush();                              // waits for full HW TX completion
        // No extra delay — flush() already blocks until all bits are on the wire.
        // Any extra time here means our driver is still ON when slave starts replying,
        // causing collision and corrupted bytes (like the 0x26 glitch we saw).
        digitalWrite(de_re, LOW);                  // back to RX immediately
        delayMicroseconds(100);                    // brief settle before reading
    }

    // Read for up to `total_ms` or until silent for `gap_ms`.
    size_t listen(uint8_t *buf, size_t cap, uint32_t total_ms, uint32_t gap_ms = 100) {
        size_t got = 0;
        uint32_t start = millis();
        uint32_t last_rx = 0;
        bool any_rx = false;
        while (millis() - start < total_ms && got < cap) {
            while (ser->available() && got < cap) {
                buf[got++] = (uint8_t)ser->read();
                last_rx = millis();
                any_rx = true;
            }
            if (any_rx && (millis() - last_rx) > gap_ms) break;
            delay(1);
        }
        return got;
    }

    void warmup() {
        for (int i = 0; i < warmup_polls; i++) {
            tx(POLL, sizeof(POLL));
            delay(warmup_gap_ms);
            while (ser->available()) (void)ser->read();
        }
        delay(after_warmup_ms);
    }

    bool do_exchange_and_parse(uint8_t v, uint8_t h, RackbusResult &out) {
        uint8_t req[20];
        size_t reqlen = rackbus::encode_vr_request(addr, v, h, req);

        tx(req, reqlen);
        delay(inter_chunk_ms);
        tx(CONT_DD, sizeof(CONT_DD));
        delay(inter_chunk_ms);
        tx(CONT_48, sizeof(CONT_48));

        static uint8_t rxbuf[256];
        size_t got = listen(rxbuf, sizeof(rxbuf), listen_ms);

        if (got == 0) return false;

        // Scan for a non-empty data frame
        bool found = false;
        rackbus::split_frames(rxbuf, got, [&](const uint8_t *frame, size_t n) {
            if (found) return;
            if (n < 4 || frame[0] != 0xCA || frame[1] != 0xC0) return;
            const uint8_t *data;
            size_t dlen;
            if (!rackbus::parse_response_frame(frame, n, data, dlen)) return;
            if (dlen == 0) return;
            // Decode
            char decoded[32];
            size_t cap = (dlen < sizeof(decoded) - 1) ? dlen : sizeof(decoded) - 1;
            for (size_t i = 0; i < cap; i++) decoded[i] = (char)(0x7F - (data[i] & 0x7F));
            decoded[cap] = '\0';
            strncpy(out.decoded, decoded, sizeof(out.decoded) - 1);
            out.decoded[sizeof(out.decoded) - 1] = '\0';
            const char *txt = nullptr;
            out.kind = rackbus::parse_value(decoded, out.value_i, out.value_f, txt);
            if (out.kind == 's') {
                strncpy(out.text, txt, sizeof(out.text) - 1);
                out.text[sizeof(out.text) - 1] = '\0';
            }
            found = true;
        });
        return found;
    }
};
