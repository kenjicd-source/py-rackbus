# Rackbus SL — Protocol Specification

Reverse-engineered protocol used by Endress+Hauser RS 485 "Rackbus SL" slaves
(Promass 63 and similar Commutec transmitters).

This document is the canonical reference for `py-rackbus`. Every formula here
has been verified against a real Promass 63 (SW 3.00.00) and against 110
transactions captured passively from an original E+H ZA672 gateway acting as
master.

---

## 1. Physical layer

| Parameter | Value |
|---|---|
| Bus  | RS-485, two-wire half-duplex |
| Baud rate | 19200 |
| Frame | 8 data bits, **no UART-level parity**, 1 stop bit (8N1) |
| Parity | Implemented in software — MSB of every byte carries the **even parity** of its low 7 bits (see §3) |
| Idle line | High (mark) — the master that holds the line idle provides the bias |
| Cable | Standard RS-485 twisted pair, shielded |
| Bus length | Up to ~1200 m at 19200 |
| Terminators | 120 Ω at far end if the cable is long; not needed for bench setups |
| Topology | Single master, up to 64 slaves on one segment |

---

## 2. Frame structure

Three classes of frame are observed on the wire:

### 2.1 Master Variable-Read request

```
+----+----+----+----------+----+------+----+------+----+------+------+----+
| CA | 22 | 5F | ADDR(...) | 5F |  V   | 5F |  H   | 72 | CRC1 | CRC2 | F2 |
+----+----+----+----------+----+------+----+------+----+------+------+----+
  SOF FUN  SEP   address    SEP value SEP value  EOP    checksum     EOM
```

| Field | Byte(s) | Meaning |
|---|---|---|
| **SOF**  | `CA` | Start of frame. Equals 7-bit `'J'` (0x4A) with parity bit set. |
| **FUN**  | `22` | Function code = "Variable Read". Equals 7-bit `'"'` (0x22), parity 0. |
| **SEP**  | `5F` | Field separator. Equals `'_'` (0x5F), parity 0. Appears 3× in this frame. |
| **ADDR** | one byte per decimal digit of the address | See §4. |
| **V**    | 1 byte | Vertical matrix index (raw integer 0..15), see §5. |
| **H**    | 1 byte | Horizontal matrix index (raw integer 0..9), see §5. |
| **EOP**  | `72` | End-of-payload. Equals `'r'` (0x72), parity 0. |
| **CRC1, CRC2** | 2 bytes | Custom 2-byte checksum, see §6. Each carries parity. |
| **EOM**  | `F2` | End of frame. Intentionally parity-violating byte. |

Slave addresses 0..9 use one ADDR byte; 10..63 use two ASCII-digit bytes
("13" = '1' then '3').

### 2.2 Slave Data response (long form)

```
+----+----+----+--------------+----+----+------+------+----+
| CA | C0 | 5F | DATA bytes…  | 03 | 72 | CRC1 | CRC2 | F2 |
+----+----+----+--------------+----+----+------+------+----+
  SOF RSP  SEP   variable len  ETX EOP   checksum    EOM
```

| Field | Byte(s) | Meaning |
|---|---|---|
| **RSP** | `C0` | Response function code. Equals `'@'` (0x40), parity 1. |
| **DATA** | 1..N bytes | The value, ASCII-encoded with parity (see §7). |
| **ETX** | `03` | End-of-text marker. Present for numeric values, **absent for
text strings (TAGs)**. |
| **EOP, CRC, EOM** | as above | The CRC formula for responses is not yet fully reversed. |

### 2.3 Slave Data response (short form)

Occasionally seen during fast multi-stage exchanges:

```
+----+----+----+--------------+----+
| CA | C0 | 5F | DATA bytes   | FF |
+----+----+----+--------------+----+
```

No CRC, no `72`. EOM is `FF` (also a parity-violating end byte).

### 2.4 Service frames

| Frame | Bytes | Role | Notes |
|---|---|---|---|
| Presence-poll | `CA 3F F2` | Master → slave (or slave-ack) | Sent every ~50 ms by ZA672 during a transaction. Without it the slave eventually stops responding. |
| Ack            | `CA 3F F2` | Slave → master                | Same wire bytes as the master poll — context distinguishes. |
| Continuation 1 | `CA DD AA 28 F2` | Master → slave | Sent after the VR request. Slave acks. Internal semantics unknown but **mandatory**. |
| Continuation 2 | `CA 48 F2` | Master → slave | Sent after continuation 1. Triggers the slave to send its DATA response. |
| Empty response | `CA C0 5F 72 A5 2E F2` | Slave → master | "No data ready" — slave sends this when the master polls too fast or before sensor refresh. |

---

## 3. Parity encoding

The wire is plain UART 8N1, but **every legitimate payload byte carries
the even parity of its low 7 bits in bit 7**:

```
parity_bit  = ones_count(byte & 0x7F) & 1
wire_byte   = (byte & 0x7F) | (parity_bit << 7)
```

Frame-delimiter bytes (`F2`, `FA`, `FF`, …) deliberately have *wrong* parity
so they cannot be mistaken for data. In practice **any byte whose high
nibble is `0xF` is treated as EOM**.

Python reference:

```python
def even_parity_bit(byte):
    n, p = byte & 0x7F, 0
    while n: p ^= n & 1; n >>= 1
    return p

def with_parity(byte):
    return (byte & 0x7F) | (even_parity_bit(byte) << 7)
```

---

## 4. Address encoding

Each decimal digit of the slave address is encoded as **`(0x5F − ASCII)`
plus parity**:

```
wire = with_parity( (0x5F - ord(digit_char)) & 0x7F )
```

| Address | ASCII digits | Per-digit `0x5F − ASCII` | Wire bytes |
|---|---|---|---|
| 0  | `'0'` (0x30) | 0x2F | `AF`         |
| 3  | `'3'` (0x33) | 0x2C | `AC`         |
| 9  | `'9'` (0x39) | 0x26 | `A6`         |
| 13 | `'1','3'` (0x31, 0x33) | 0x2E, 0x2C | `2E AC`   |
| 63 | `'6','3'` (0x36, 0x33) | 0x29, 0x2C | `A9 AC`   |

Range: 0..63. Promass 63 hardware limits the address to 0..63, and the
gateway rejects values ≥64 with `>!07,Addr.err.` (when reachable through
the ZA672 CLI).

---

## 5. V and H encoding

V (vertical, 0..15) and H (horizontal, 0..9) of the parameter matrix are
**raw integers**, complemented against 0x5F, with parity:

```
wire = with_parity( (0x5F - value) & 0x7F )
```

| V or H | `(0x5F − value)` | Wire |
|---|---|---|
| 0  | 0x5F | `5F` |
| 1  | 0x5E | `DE` |
| 2  | 0x5D | `DD` |
| 3  | 0x5C | `5C` |
| 4  | 0x5B | `DB` |
| 5  | 0x5A | `5A` |
| 9  | 0x56 | `56` |
| 10 (=A) | 0x55 | `55` |

Note that the V/H encoding uses the **raw integer**, while the address
encoding uses **ASCII digit characters**. The same `0x5F` constant is used
in both — context (single-byte raw vs. multi-byte ASCII) determines the
interpretation.

---

## 6. CRC for requests

Reverse-engineered formula, verified on **110 transactions**:

```
crc1 = 0x2E XOR (0x08 if odd count of MSB-set bytes in body else 0)
crc2 = (sum(byte & 0x7F for byte in body) + 0x48) mod 256
```

`body` is everything from `CA` (SOF) up to and including the EOP `72`,
but **excluding** the CRC bytes and the EOM `F2`.

Both CRC bytes are then parity-augmented (MSB = even parity) before going
on the wire.

### Worked example for `VR 3, 0, 0`

Body (9 bytes): `CA 22 5F AC 5F 5F 5F 5F 72`

| Step | Value |
|---|---|
| MSB-set bytes in body | `CA`, `AC` → count = 2 (even) |
| `crc1` | `0x2E XOR 0 = 0x2E` |
| Sum of `byte & 0x7F` | 0x4A + 0x22 + 0x5F + 0x2C + 0x5F + 0x5F + 0x5F + 0x5F + 0x72 = 0x2E5 |
| `crc2` | `(0xE5 + 0x48) mod 256 = 0x2D` |
| Wire | `2E` (parity 0), `2D` (parity 0) |

Full frame: `CA 22 5F AC 5F 5F 5F 5F 72 2E 2D F2`. ✓

### Worked example for `VR 3, 0, 1`

Body: `CA 22 5F AC 5F 5F 5F DE 72`

| Step | Value |
|---|---|
| MSB-set bytes | `CA`, `AC`, `DE` → count = 3 (odd) |
| `crc1` raw | `0x2E XOR 0x08 = 0x26` |
| Sum low-7 | 0x4A + 0x22 + 0x5F + 0x2C + 0x5F + 0x5F + 0x5F + 0x5E + 0x72 = 0x2E4 |
| `crc2` raw | `(0xE4 + 0x48) mod 256 = 0x2C` |
| Wire (with parity) | `A6 AC` |

Full frame: `CA 22 5F AC 5F 5F 5F DE 72 A6 AC F2`. ✓

### Reference implementation

```python
def compute_request_crc(body):
    msb_count = sum(1 for b in body if b & 0x80)
    crc1 = 0x2E ^ (0x08 if msb_count % 2 else 0x00)
    crc2 = (sum(b & 0x7F for b in body) + 0x48) & 0xFF
    return crc1, crc2
```

---

## 7. CRC for responses

**Not yet fully reverse-engineered.**

Empirically the response CRC seems to be a similar 2-byte custom checksum,
but the constants do not match the request formula. The `py-rackbus`
implementation currently **does not verify** response CRCs — the slave is
trusted. Data integrity is implicitly checked because malformed data
either parses cleanly into a wrong value or fails to parse at all.

`code/crc.py::verify_response_crc()` is a stub that always returns `True`;
patch it once the formula is known.

---

## 8. Decoding the DATA payload

Each data byte decodes to one ASCII character:

```
char = chr(0x7F - (wire_byte & 0x7F))
```

The decoded string follows these rules:

| Last char of decoded string | Meaning | Value rule |
|---|---|---|
| `' '` (0x20, space) | Integer | `int(body)` |
| `'!' '"' '#' '$' '%' '&' "'" '(' ')' '*' '+' ',' '-' '.' '/'` (0x21..0x2F) | Float with implicit decimal point | `decimal_places = ord(last) − 0x20`, value = `int(body) / 10**decimal_places` |
| `'@'` (0x40, single char and that's the whole payload) | Invalid / not applicable | `None` |
| Anything else | Plain string (TAG, identifier) | `decoded` as-is |

The leading `'-'` (if any) is a sign character — strip and negate after
parsing.

### Worked examples

| Wire `<data>` | Decoded | Final value |
|---|---|---|
| `CF CF DE` | `'00!'` | `0.0` (digits `"00"`, decimal places = 1) |
| `CA DE` | `'5!'` | `0.5` |
| `4D CC CA C6 DD` | `'2359"'` | `23.59` (decimal places = 2) |
| `4D 4E 4E C9 C9 DB` | `'21166$'` | `2.1166` (decimal places = 4) |
| `4D 4B CC CF 47 CA 48 55` | `'2430857*'` | `0.0002430857` (decimal places = 10) |
| `D2 4E 4E 48 5F` | `'-117 '` | `-117` (trailing space → int) |
| `CC 5F` | `'3 '` | `3` |
| `BB 3A B2 30 5F 2B BE B8` | `'DEMO TAG'` | `'DEMO TAG'` (text, no scale marker) |
| `3F` (single byte) | `'@'` | `None` (invalid) |

### Reference implementation

```python
def parse_value(decoded):
    if not decoded or decoded == '@':
        return None
    last, body = decoded[-1], decoded[:-1]
    if last == ' ':
        return int(body) if body.lstrip('-').isdigit() else body
    if 0x21 <= ord(last) <= 0x2F:
        dp = ord(last) - 0x20
        sign = -1 if body.startswith('-') else 1
        return sign * int(body.lstrip('-')) / (10 ** dp)
    return decoded
```

---

## 9. Timing requirements

Measured against ZA672 + Promass 63 on a clean bench (DN50 sensor):

| Parameter | Value | Notes |
|---|---|---|
| UART byte time | 520 µs | 10 bits at 19200 baud |
| Inter-chunk delay (master) | **50 ms** | Between VR / CONT1 / CONT2 |
| Slave response delay | < 30 ms | After CONT2 |
| Listen window after an exchange | 500 ms | Covers all retries / empty responses |
| Idle warmup poll interval | 50 ms | 5 polls (= 250 ms) before sleeping slave responds |
| Per-transaction time | 200..400 ms | Whole VR → DATA cycle |

Below ~10 ms inter-chunk the slave starts returning empty responses
even after warmup. Above ~200 ms it sometimes drops back to "asleep"
state and needs another warmup before responding.

---

## 10. Multi-stage exchange pattern

```
Master                                    Slave
   |                                        |
   | --- 5× CA 3F F2 (presence polls) -->   |   (only if bus was idle)
   |                                        |
   | --- VR request (12 bytes) ----------> |
   |                                        |
   |       wait 50 ms                       |
   |                                        |
   | --- CA DD AA 28 F2 (continuation 1) -> |
   |                                        |
   |           <-- CA 3F F2 (ack) --------  |
   |                                        |
   |       wait 50 ms                       |
   |                                        |
   | --- CA 48 F2 (continuation 2) -------> |
   |                                        |
   |           <-- CA C0 5F 72 A5 2E F2     |   "not ready yet"
   |               (empty response)         |
   |                                        |
   | [retry the whole sequence]             |
   |                                        |
   |           <-- CA C0 5F CF CF DE 03 72 2E 21 F2  (DATA)
   |                                        |
```

The "empty" response on the first attempt is normal — Promass needs about
one full sensor cycle (~100..300 ms) to produce data after a fresh poll.
Always be prepared to retry once.

---

## 11. Edge cases & gotchas

- **`'@'` value (`3F` byte alone in payload).** Slave-reported invalid
  parameter. Don't treat as zero. Return `None`. Examples: density on
  Empty-pipe state, an empty totalizer, an un-assigned output channel.

- **Empty-pipe alarm.** Triggers `0.2 g/cm³` density (which is actually
  *real* — that's the configured EPD threshold) and `0.0` mass flow.
  Calculated density (`V1H2`) returns `'@'`.

- **Bus idle > ~1 s → slave asleep.** First VR after silence returns
  acks only. Five presence-polls bring it back. The client's `_warmup()`
  handles this transparently.

- **Address > 63.** ZA672 rejects with `>!07,Addr.err.`. The bare slave
  silently ignores out-of-range addresses.

- **Trailing parity-violating bytes in stream.** Don't assume `F2` is the
  only EOM. `FA` (used by some empty responses) and `FF` (short form)
  also terminate frames. Treat any high-nibble-`0xF` byte as EOM.

- **CRC byte parity.** CRC bytes themselves carry parity. When comparing
  to the formula, strip MSB first or compare against `with_parity(crc)`.

- **Text values have no ETX.** Numeric responses end with `03 72 …`;
  text responses (TAG) end directly with `72 …`. The parser handles both.

- **First request after `serial.Serial.open()`.** The OS may queue some
  garbage in the RX buffer of the FTDI/CP2102/CH340 driver. Always
  `reset_input_buffer()` before the first transaction, or simply rely on
  the warmup polls to flush.

- **CONT_DD = `CA DD AA 28 F2`** — internal master command, semantics
  unknown but verified mandatory. Sending only `VR` + `CA 48 F2` is not
  enough.

---

## 12. Quick reference card

```
Frame markers:
    SOF  = CA       (J + parity)
    EOM  = F2 / FA / FF   (any high-nibble-F byte)
    SEP  = 5F       (_ + parity 0)
    EOP  = 72       (r + parity 0)
    ETX  = 03       (only in numeric responses, before EOP)

Functions:
    22 = Variable Read (request)
    C0 = Variable Read response

Encodings:
    Address digit chars : wire = (0x5F - ASCII) + parity
    V/H raw integers    : wire = (0x5F - value) + parity
    DATA chars          : wire = (0x7F - ASCII) + parity

CRC (requests):
    crc1 = 0x2E XOR (0x08 if odd-MSB-count else 0)
    crc2 = (sum_low7 + 0x48) mod 256
    Both bytes then receive parity.

Multi-stage exchange:
    [warmup 5× POLL]  VR  →  50ms  →  CA DD AA 28 F2  →  50ms
                       →  CA 48 F2  →  listen 500ms

Retry on empty response (CA C0 5F 72 A5 2E F2).
```
