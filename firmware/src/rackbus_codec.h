// Rackbus SL frame encoding/decoding — C++ port of rackbus_client/codec.py.
//
// Encoding rules:
//   - Each payload byte: 7 bits data + 1 even-parity bit in MSB.
//   - Address: ASCII digit chars with `(0x5F - char)` complement + parity.
//   - V/H: raw integers with `(0x5F - value)` complement + parity.
//   - Data chars in responses: `(0x7F - char)` complement + parity.
//
// All formulas verified against 110 captured ZA672 transactions.

#pragma once
#include <Arduino.h>

namespace rackbus {

// --- parity helpers ---
inline uint8_t even_parity_bit(uint8_t b) {
    uint8_t n = b & 0x7F, p = 0;
    while (n) { p ^= (n & 1); n >>= 1; }
    return p;
}
inline uint8_t with_parity(uint8_t b) {
    return (b & 0x7F) | (even_parity_bit(b) << 7);
}

// --- char/value ↔ wire ---
inline char wire_to_char(uint8_t b) {
    return (char)(0x7F - (b & 0x7F));
}

// --- Request CRC ---
// crc1 = 0x2E XOR (0x08 if odd MSB-set count in body else 0)
// crc2 = (sum(b & 0x7F for b in body) + 0x48) mod 256
inline void compute_request_crc(const uint8_t *body, size_t n,
                                uint8_t &crc1, uint8_t &crc2) {
    uint16_t msb_count = 0;
    uint16_t sum_low7 = 0;
    for (size_t i = 0; i < n; i++) {
        if (body[i] & 0x80) msb_count++;
        sum_low7 += body[i] & 0x7F;
    }
    crc1 = 0x2E ^ ((msb_count & 1) ? 0x08 : 0x00);
    crc2 = (uint8_t)((sum_low7 + 0x48) & 0xFF);
}

// --- Encode VR (Variable Read) request ---
// Writes the full frame into `out` (caller-allocated, ≥16 bytes is plenty).
// Returns the number of bytes written. Address can be 1-2 ASCII digits.
inline size_t encode_vr_request(uint8_t addr, uint8_t v, uint8_t h, uint8_t *out) {
    size_t i = 0;
    out[i++] = 0xCA;                                    // SOF
    out[i++] = with_parity(0x22);                       // FUN '"' (read)
    out[i++] = 0x5F;                                    // separator

    // address as ASCII digits with 0x5F-base complement
    char buf[4]; size_t n = snprintf(buf, sizeof(buf), "%u", (unsigned)addr);
    for (size_t k = 0; k < n; k++) {
        out[i++] = with_parity((0x5F - (uint8_t)buf[k]) & 0xFF);
    }

    out[i++] = 0x5F;                                    // separator
    out[i++] = with_parity((0x5F - v) & 0xFF);          // V (raw int)
    out[i++] = 0x5F;                                    // separator
    out[i++] = with_parity((0x5F - h) & 0xFF);          // H
    out[i++] = with_parity(0x72);                       // end-of-payload 'r'

    // CRC (over everything from SOF to '72' inclusive)
    uint8_t crc1, crc2;
    compute_request_crc(out, i, crc1, crc2);
    out[i++] = with_parity(crc1);
    out[i++] = with_parity(crc2);

    out[i++] = 0xF2;                                    // EOF (parity-violating)
    return i;
}

// --- Parse a single response frame ---
//   Long  : CA C0 5F <data> [03] 72 CRC1 CRC2 <EOF=Fx>
//   Short : CA C0 5F <data> <EOF=Fx>
// On success returns true and sets data_ptr/data_len pointing into the input.
// Caller must keep `frame` alive while using data_ptr.
inline bool parse_response_frame(const uint8_t *frame, size_t n,
                                 const uint8_t *&data_ptr, size_t &data_len) {
    if (n < 4 || frame[0] != 0xCA || frame[1] != 0xC0 || frame[2] != 0x5F)
        return false;
    uint8_t last = frame[n - 1];
    if ((last & 0xF0) != 0xF0)
        return false;
    if (n >= 7 && frame[n - 4] == 0x72) {
        // long format: 72 CRC1 CRC2 EOF
        data_ptr = frame + 3;
        data_len = n - 7;            // exclude 72 CRC1 CRC2 EOF
    } else {
        data_ptr = frame + 3;
        data_len = n - 4;            // exclude EOF
    }
    // Trim trailing 0x03 (ETX) if present
    if (data_len > 0 && data_ptr[data_len - 1] == 0x03) data_len--;
    return true;
}

// --- Decode data bytes to ASCII (writes null-terminator) ---
inline void decode_data(const uint8_t *data, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) out[i] = (char)(0x7F - (data[i] & 0x7F));
    out[n] = '\0';
}

// --- Parse value from decoded ASCII ---
// Strategy matches the Python `parse_value()`.
//   - Empty / '@' → return false (invalid)
//   - Trailing space → integer, sets *out_int
//   - Trailing 0x21..0x2F → float with scale, sets *out_float
//   - Otherwise → text, sets out_str
//
// `out_kind` returns one of: 'i' int, 'f' float, 's' string, 'n' invalid.
inline char parse_value(const char *decoded,
                        long &out_int, double &out_float, const char *&out_str) {
    size_t n = strlen(decoded);
    if (n == 0 || (n == 1 && decoded[0] == '@')) return 'n';
    char last = decoded[n - 1];
    if (last == ' ') {
        // integer
        char *end;
        long v = strtol(decoded, &end, 10);
        out_int = v;
        return 'i';
    }
    if ((uint8_t)last >= 0x21 && (uint8_t)last <= 0x2F) {
        // float with scale
        int dp = (int)last - 0x20;
        // body = decoded[0..n-2]
        char body[32];
        size_t blen = (n - 1 < sizeof(body) - 1) ? n - 1 : sizeof(body) - 1;
        memcpy(body, decoded, blen);
        body[blen] = '\0';
        long sign = 1;
        const char *p = body;
        if (*p == '-') { sign = -1; p++; }
        long mantissa = strtol(p, nullptr, 10);
        double scale = 1.0;
        for (int i = 0; i < dp; i++) scale *= 10.0;
        out_float = (double)(sign * mantissa) / scale;
        return 'f';
    }
    // text
    out_str = decoded;
    return 's';
}

// --- Split a wire stream into frames ---
// `cb(frame, len)` is called once per frame found.
// Frame boundary = byte with high nibble 0xF (any of F0..FF).
template <typename Cb>
inline void split_frames(const uint8_t *wire, size_t n, Cb cb) {
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if ((wire[i] & 0xF0) == 0xF0) {
            cb(wire + start, i - start + 1);
            start = i + 1;
        }
    }
    if (start < n) cb(wire + start, n - start);
}

} // namespace rackbus
