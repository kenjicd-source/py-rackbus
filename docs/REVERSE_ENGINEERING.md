# Notes & lessons learned

A grab-bag of observations from the reverse engineering, organized by
theme. Useful both for future contributors and for anyone wondering
*why* the code looks the way it does.

---

## What was hard, and how we got past it

### "Slave is dead — nothing comes back"

The very first thing tried was a custom Python prober that swept
hypotheses for Modbus / FT3 / variants of CRC-16, all at 9600 8N1. Six
hundred frames went out, **zero replies** came back. The conclusion
"Promass is broken" looked attractive but was wrong on three counts:

1. **Wrong baud rate.** Rackbus runs at 19200, not 9600. At 9600 the
   slave does not even see legible bits.
2. **No master on the bus.** Rackbus slaves are pure responders — they
   never speak until polled by a valid master frame. Sniffing without
   a master means silence forever.
3. **Wrong framing.** Plain Modbus / CCITT CRC-16 are wrong. The
   protocol is custom.

The fix was *not* to keep guessing. It was to introduce a known-good
master (the Endress+Hauser ZA672 gateway), capture the real wire
traffic, and reverse from data. The probe scripts were retired.

### "Why does the gateway answer `RS232 err.` to every command?"

The ZA672 gateway exposes an ASCII CLI on its DB25 RS-232 port. Early
attempts at `INFO`, `HELP`, `R`, `*IDN?`, etc. all returned
`>!05,1,RS232 err.`. The trick was: the gateway's CLI uses **7E1**
framing, but we were sending 8N1. CH340 USB-to-RS232 adapters
mis-handle pyserial 7E1 on the receive side, so the cleanest workaround
is:

- Keep pyserial at 8N1.
- Compute the even-parity bit in software and stuff it into the MSB of
  every outgoing byte.
- Strip the MSB on every incoming byte.

After that switch, the CLI immediately accepted `INFO` and revealed the
device identity (`ZA672 (D),V1.7`) and port settings
(`>#82,96,1,E,7,(bd,stop,par,data)`). This nailed down the framing and
unlocked everything that followed.

### "ZA672 answers `>!06 Syntax err` to *every* command"

Once parity was right, the gateway responded to *something* in the
right shape, but READ / GET / SCAN / POLL / MODBUS / `>3,0,0` / `?42`
all returned syntax error. The CLI vocabulary turned out to be tiny:
**only `INFO`, `VERSION`, `RESET`, `VR`, `VW`, `VRB`, `LST`, `APP`,
`INS`, `OVW`, `DEL`** are accepted. The decisive clue was a
publicly available technical datasheet for the gateway, which gave one
example transaction:

```
VR 36, 0, 0
36, 245.7
```

That single example was worth more than three rounds of guessing.
**Lesson: when stuck on a CLI, find any vendor doc — even a marketing
brochure may contain a worked example.**

### "Why does direct VR over RS-485 return only acks?"

When the gateway was removed and our client started talking RS-485
directly to Promass, the slave acknowledged frames (`CA 3F F2`) but
sent no data. The recipe ZA672 uses is a **multi-stage exchange**:

```
warmup (5× CA 3F F2)  →  VR  →  CA DD AA 28 F2  →  CA 48 F2  →  listen
```

The continuation frames `DD AA 28` and `48` are not documented anywhere
we could find. They were extracted by capturing 110 transactions ZA672
generated while we drove its CLI. **Lesson: when the slave is silent
but acks individual frames, the master probably owes the slave a
sequence of follow-up frames the master normally hides.**

### "First attempt is always empty"

After the multi-stage exchange was working, the first response after a
fresh open or a long idle is `CA C0 5F 72 A5 2E F2` — a content-free
data frame meaning "not ready". A second attempt almost always
succeeds. We added an unconditional `_warmup()` of five presence-polls
+ up to 5 retries; in practice 2 retries cover every observed case.

---

## Hypotheses that turned out to be wrong

For future readers who may try the same dead ends:

| Hypothesis | Why it was tempting | Why it's wrong |
|---|---|---|
| "Promass uses Modbus" | E+H sells Modbus variants. | This unit is the Rackbus SL variant — see `V6H1`. |
| "Just try CRC-16 polynomials with `reveng`" | Standard tool, two-byte trailers. | Not CRC-16. It's a custom 2-byte additive+XOR checksum. `reveng` found nothing. |
| "Sniff without a master, the device will say hello" | Some buses chatter. | Pure responder. Idle forever. |
| "The MSB-1 bytes are a 'marker' " | They look special. | They are even-parity bits. |
| "`5F` everywhere is data" | It is literal `'_'` in ASCII. | It's a field separator *and* the complement target — both. |
| "The trailing `'!'` / `'$'` is part of the number string" | They appear at the end of every float. | It's a scale code: `decimal_places = ord(last) - 0x20`. |
| "ZA672 = FXA192" | A previous handoff doc called the gateway "FXA192". | It is a ZA672. The first `INFO` reply self-identifies. |

---

## Empirical performance

Measured on the test bench, single VR transaction (warmup + exchange + listen):

| Scenario | Time |
|---|---|
| Cold open, first VR | 300–500 ms (typically one retry) |
| Warm bus, subsequent VR | 250–300 ms |
| Bulk read of 11 parameters (`read_all`) | 3–5 s |

These are the ballpark numbers; tightening the `inter_chunk_ms` below
30 ms makes the slave start returning empties, increasing it above
~150 ms occasionally causes the bus to re-idle and need warmup again.

The protocol is not designed for high-frequency telemetry. ZA672 itself
caches in an "auto-scan buffer" if you need sub-second cadence — see
the gateway's technical datasheet.

---

## Stability

- ~1 hour of continuous reads, no failed transactions after retries.
- Survives Promass power-cycling: a few seconds after re-power the
  client retries succeed.
- No persistent state on the master side — closing and reopening the
  port is safe at any time.

---

## "What does `CF CF DE` mean?"

Quick decoder for anyone staring at a sniff:

```
For each wire byte b:
    char = chr(0x7F - (b & 0x7F))      # strip parity, invert
```

So:
- `CF` → `0x7F - 0x4F = 0x30` → `'0'`
- `DE` → `0x7F - 0x5E = 0x21` → `'!'`

So `CF CF DE` is `"00!"`, which under the scale-char rule means
`int("00") / 10**(ord('!') - 0x20) = 0 / 10 = 0.0`.

Examples worth memorizing:
```
CF              '0'
4D              '2'
CC              '3'
CA              '5'
C6              '9'
D2              '-'
5F              ' '   (trailing → integer)
!  " # $ % * → decimal point shift 1, 2, 3, 4, 5, 10
@              invalid / not applicable
```

---

## Where to take this next

- **Response CRC.** Still not reversed. The 110-transaction dump
  (`matrix_dump_20260526_201839.json`) has every empty/non-empty pair
  needed; this is a job for someone with patience and `reveng`.
- **`CA DD AA 28 F2` semantics.** Empirically a "continue read" token.
  What its bytes actually encode is unknown.
- **ESP32 port.** The codec is ~50 lines of trivial arithmetic; the
  transport is `HardwareSerial` plus `millis()` waits. Use MAX485
  (RS-485), **not** MAX232 (RS-232).
- **VW (Variable Write).** ZA672 supports it via the CLI but the wire
  format was not captured in this round. The structure is presumably
  symmetric to VR, with a different FUN code and an extra data field.
- **Other Commutec devices.** Promass 60/63/83/etc., Levelflex, Cerabar
  Rackbus variants should all speak the same wire protocol. Address
  selection and the matrix layout differ per device.
