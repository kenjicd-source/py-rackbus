# Example transactions — Rackbus SL

Real captured frames between an Endress+Hauser ZA672 (acting as master)
and a Promass 63 (slave at address 3). Source: `matrix_dump_20260526_201839.json`.

All values match the local display and were verified during the reverse
engineering effort. Every TX frame here is identical (byte-for-byte) to
what our `encode_vr_request()` produces.

---

## V0H0 mass flow (zero)

**Symbolic name:** `MASS_FLOW`
**Bus address:** 3
**Expected type:** `float`
**ZA672 RS232 reply (ground truth):** `3,0.0`

### Master → Slave (request)

```
CA 22 5F AC 5F 5F 5F 5F 72 2E 2D F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `5F` — V=0 encoded (`0x5F-0` + parity)
- `5F` — separator
- `5F` — H=0 encoded (`0x5F-0` + parity)
- `72` — EOP
- `2E 2D` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F 5F 5F 5F 72 2E 2D F2 (master TX — echo from passive sniff)
CA 22 5F AC 5F 5F 5F 5F 72 2E 2D F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F CF CF DE 03 72 2E 21 F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `CF CF DE`
**Decoded text (chars):** `'00!'`
**Final value:** `0.0` ✓

---

## V0H1 totalizer (small float)

**Symbolic name:** `TOTALIZER_1`
**Bus address:** 3
**Expected type:** `float`
**ZA672 RS232 reply (ground truth):** `3,0.0002430857`

### Master → Slave (request)

```
CA 22 5F AC 5F 5F 5F DE 72 A6 AC F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `5F` — V=0 encoded (`0x5F-0` + parity)
- `5F` — separator
- `DE` — H=1 encoded (`0x5F-1` + parity)
- `72` — EOP
- `A6 AC` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F 5F 5F DE 72 A6 AC F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 4D 4B CC CF 47 CA 48 55 03 72 24 2B F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `4D 4B CC CF 47 CA 48 55`
**Decoded text (chars):** `'2430857*'`
**Final value:** `0.0002430857` ✓

---

## V1H0 density (small float)

**Symbolic name:** `DENSITY`
**Bus address:** 3
**Expected type:** `float`
**ZA672 RS232 reply (ground truth):** `3,0.2`

### Master → Slave (request)

```
CA 22 5F AC 5F DE 5F 5F 72 A6 AC F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `DE` — V=1 encoded (`0x5F-1` + parity)
- `5F` — separator
- `5F` — H=0 encoded (`0x5F-0` + parity)
- `72` — EOP
- `A6 AC` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F DE 5F 5F 72 A6 AC F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 4D DE 03 72 28 AF F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `4D DE`
**Decoded text (chars):** `'2!'`
**Final value:** `0.2` ✓

---

## V1H1 temperature (decimal)

**Symbolic name:** `TEMPERATURE`
**Bus address:** 3
**Expected type:** `float`
**ZA672 RS232 reply (ground truth):** `3,23.59`

### Master → Slave (request)

```
CA 22 5F AC 5F DE 5F DE 72 2E 2B F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `DE` — V=1 encoded (`0x5F-1` + parity)
- `5F` — separator
- `DE` — H=1 encoded (`0x5F-1` + parity)
- `72` — EOP
- `2E 2B` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F DE 5F DE 72 2E 2B F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 4D CC CA C6 DD 03 72 2E 2D F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `4D CC CA C6 DD`
**Decoded text (chars):** `'2359"'`
**Final value:** `23.59` ✓

---

## V2H4 bus address (int)

**Symbolic name:** `BUS_ADDRESS`
**Bus address:** 3
**Expected type:** `int`
**ZA672 RS232 reply (ground truth):** `3,3`

### Master → Slave (request)

```
CA 22 5F AC 5F DD 5F DB 72 2E 27 F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `DD` — V=2 encoded (`0x5F-2` + parity)
- `5F` — separator
- `DB` — H=4 encoded (`0x5F-4` + parity)
- `72` — EOP
- `2E 27` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F DD 5F DB 72 2E 27 F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F CC 5F 03 72 28 AF F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `CC 5F`
**Decoded text (chars):** `'3 '`
**Final value:** `3` ✓

---

## V9H1 zero point (negative)

**Symbolic name:** `ZERO_POINT`
**Bus address:** 3
**Expected type:** `int`
**ZA672 RS232 reply (ground truth):** `3,-117`

### Master → Slave (request)

```
CA 22 5F AC 5F 56 5F DE 72 A6 A3 F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `56` — V=9 encoded (`0x5F-9` + parity)
- `5F` — separator
- `DE` — H=1 encoded (`0x5F-1` + parity)
- `72` — EOP
- `A6 A3` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F 56 5F DE 72 A6 A3 F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F D2 4E 4E 48 5F 03 72 27 AC F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `D2 4E 4E 48 5F`
**Decoded text (chars):** `'-117 '`
**Final value:** `-117` ✓

---

## V9H2 nominal size (int)

**Symbolic name:** `NOMINAL_SIZE`
**Bus address:** 3
**Expected type:** `float`
**ZA672 RS232 reply (ground truth):** `3,50.0`

### Master → Slave (request)

```
CA 22 5F AC 5F 56 5F DD 72 A6 22 F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `56` — V=9 encoded (`0x5F-9` + parity)
- `5F` — separator
- `DD` — H=2 encoded (`0x5F-2` + parity)
- `72` — EOP
- `A6 22` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F 56 5F DD 72 A6 22 F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F CA CF CF DE 03 72 AA AC F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `CA CF CF DE`
**Decoded text (chars):** `'500!'`
**Final value:** `50.0` ✓

---

## V9H0 calibration factor

**Symbolic name:** `CALIBR_FACTOR`
**Bus address:** 3
**Expected type:** `float`
**ZA672 RS232 reply (ground truth):** `3,2.1166`

### Master → Slave (request)

```
CA 22 5F AC 5F 56 5F 5F 72 2E 24 F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `56` — V=9 encoded (`0x5F-9` + parity)
- `5F` — separator
- `5F` — H=0 encoded (`0x5F-0` + parity)
- `72` — EOP
- `2E 24` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F 56 5F 5F 72 2E 24 F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 4D 4E 4E C9 C9 DB 03 72 2B 2E F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `4D 4E 4E C9 C9 DB`
**Decoded text (chars):** `'21166$'`
**Final value:** `2.1166` ✓

---

## V10H0 (=VAH0) tag (string)

**Symbolic name:** `TAG_NUMBER`
**Bus address:** 3
**Expected type:** `string`
**Example tag value:** `DEMO TAG` (8 ASCII chars; this is a synthetic
placeholder — the actual tag in the test device is anonymized for
publication)

### Master → Slave (request)

```
CA 22 5F AC 5F 55 5F 5F 72 2E A3 F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `55` — V=10 encoded (`0x5F-10` + parity)
- `5F` — separator
- `5F` — H=0 encoded (`0x5F-0` + parity)
- `72` — EOP
- `2E A3` — checksum
- `F2` — EOM

### DATA payload encoding

For string parameters the DATA section is the tag text encoded
character-by-character with the standard data rule
`wire = with_parity(0x7F - char)`. No ETX (`0x03`) marker is emitted for
text values — the EOP `0x72` follows the payload directly.

For `'DEMO TAG'`:

| Char | ASCII | `0x7F - ASCII` | Wire byte (with parity) |
|---|---|---|---|
| `D` | 0x44 | 0x3B | `BB` |
| `E` | 0x45 | 0x3A | `3A` |
| `M` | 0x4D | 0x32 | `B2` |
| `O` | 0x4F | 0x30 | `30` |
| ` ` | 0x20 | 0x5F | `5F` |
| `T` | 0x54 | 0x2B | `2B` |
| `A` | 0x41 | 0x3E | `BE` |
| `G` | 0x47 | 0x38 | `B8` |

**Decoded payload bytes:** `BB 3A B2 30 5F 2B BE B8`
**Decoded text (chars):** `'DEMO TAG'`
**Final value:** `'DEMO TAG'` ✓

The full response frame structure is identical to other long-form
responses (`CA C0 5F <payload> 72 <crc1> <crc2> F2`) but with no `0x03`
ETX before the EOP. The response checksum formula is not yet fully
reverse-engineered (see section 7 of `FINAL_PROTOCOL.md`).

---

## V2H6 SW version (large int)

**Symbolic name:** `SW_VERSION_COM`
**Bus address:** 3
**Expected type:** `int`
**ZA672 RS232 reply (ground truth):** `3,30000`

### Master → Slave (request)

```
CA 22 5F AC 5F DD 5F 59 72 A6 A5 F2
```

**Breakdown:**
- `CA` — SOF
- `22` — FUN (Variable Read)
- `5F` — separator
- `AC` — address 3 encoded (`'3'` → `0x5F-0x33=0x2C` → with parity `0xAC`)
- `5F` — separator
- `DD` — V=2 encoded (`0x5F-2` + parity)
- `5F` — separator
- `59` — H=6 encoded (`0x5F-6` + parity)
- `72` — EOP
- `A6 A5` — checksum
- `F2` — EOM

### Slave → Master (transaction)

```
CA 22 5F AC 5F DD 5F 59 72 A6 A5 F2 (master TX — echo from passive sniff)
CA 3F F2 (ack / poll)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F 72 A5 2E F2 (empty response)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
CA 48 F2 (continuation 2 — master)
CA C0 5F CC CF CF CF CF 5F 03 72 AC AF F2 (DATA)
CA DD AA 28 F2 (continuation 1 — master)
CA 3F F2 (ack / poll)
```

**Decoded payload bytes:** `CC CF CF CF CF 5F`
**Decoded text (chars):** `'30000 '`
**Final value:** `30000` ✓

---

