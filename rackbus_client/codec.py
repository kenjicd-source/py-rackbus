"""
Frame encoding/decoding for the Rackbus SL protocol.

Wire encoding:
  - Each payload byte carries 7 bits of data + 1 even-parity bit in MSB.
  - Char encoding: wire_byte = (0x7F - char) | even_parity_bit  for DATA in responses.
  - Address chars use complement base 0x5F (not 0x7F).
  - V/H values: same 0x5F-complement but applied to RAW int (0..15), not to ASCII.
  - Inverse of data decoding: char = 0x7F - (wire_byte & 0x7F).

Frame structures:
  Request (master → slave):
      CA  22  5F  <addr>  5F  <V>  5F  <H>  72  CRC1  CRC2  F2
  Response (slave → master, long):
      CA  C0  5F  <data>  03  72  CRC1  CRC2  F2
  Response (slave → master, short — without CRC):
      CA  C0  5F  <data>  FF
  Master polls / continuations:
      CA  3F  F2          (presence poll)
      CA  DD AA 28  F2    (continuation 1)
      CA  48  F2          (continuation 2)
  Ack from slave:
      CA  3F  F2          (same wire as master poll — context distinguishes)

All EOF markers have high nibble 0xF (observed: F2, FA, FF).
"""
from __future__ import annotations

from typing import Optional, Tuple

from .crc import compute_request_crc


# --- Parity helpers ---

def even_parity_bit(byte: int) -> int:
    """Return 1 if the 7 LSBs of `byte` have an odd count of ones (i.e.
    the parity bit needed in the MSB to make the total even).
    """
    n, p = byte & 0x7F, 0
    while n:
        p ^= n & 1
        n >>= 1
    return p


def with_parity(byte: int) -> int:
    """Augment a 7-bit value with its even-parity bit in the MSB."""
    return (byte & 0x7F) | (even_parity_bit(byte) << 7)


# --- Char ↔ wire (DATA encoding, 0x7F-based) ---

def char_to_wire(c) -> int:
    """ASCII char → wire byte using complement-against-0x7F + parity.

    Used for DATA in response frames. NOT for address (use the inline
    0x5F-based formula in encode_vr_request()).
    """
    if isinstance(c, str):
        c = ord(c)
    return with_parity((0x7F - c) & 0x7F)


def wire_to_char(b: int) -> str:
    """Wire byte → ASCII char (strips MSB, applies inverse complement)."""
    return chr(0x7F - (b & 0x7F))


# --- Frame split ---

def split_frames(wire: bytes) -> list:
    """Split a wire byte stream into Rackbus frames.

    EOF markers are bytes with high nibble = 0xF (observed F2, FA, FF).
    """
    out = []
    cur = bytearray()
    for b in wire:
        cur.append(b)
        if (b & 0xF0) == 0xF0:
            out.append(bytes(cur))
            cur = bytearray()
    if cur:
        out.append(bytes(cur))
    return out


# --- Request encoding ---

def encode_vr_request(addr: int, v: int, h: int) -> bytes:
    """Encode a Variable Read request: VR <addr>,<V>,<H>.

    Address chars use complement base 0x5F.
    V and H are RAW integers (0..15) also complemented against 0x5F.
    Each payload byte carries even-parity in MSB.
    """
    body = bytearray()
    body.append(0xCA)                                          # SOF
    body.append(with_parity(0x22))                             # function '"' (read)
    body.append(0x5F)                                          # separator '_'
    for ch in str(addr):                                       # addr ASCII digit(s)
        body.append(with_parity((0x5F - ord(ch)) & 0xFF))      # 0x5F base!
    body.append(0x5F)                                          # separator
    body.append(with_parity((0x5F - v) & 0xFF))                # V (raw int)
    body.append(0x5F)
    body.append(with_parity((0x5F - h) & 0xFF))                # H
    body.append(with_parity(0x72))                             # end-of-payload 'r'

    crc1, crc2 = compute_request_crc(bytes(body))
    body.append(with_parity(crc1))
    body.append(with_parity(crc2))
    body.append(0xF2)                                          # EOF (parity-violating)
    return bytes(body)


# --- Response parsing ---

def parse_response_frame(frame: bytes) -> Optional[Tuple[bytes, Optional[bytes]]]:
    """Parse a single response frame.

    Long format:  CA C0 5F <data> [03] 72 CRC1 CRC2 <EOF>
    Short format: CA C0 5F <data> <EOF>

    Returns (data_bytes, crc_bytes_or_None) or None if malformed.
    """
    if len(frame) < 4 or frame[0] != 0xCA or frame[1] != 0xC0 or frame[2] != 0x5F:
        return None
    last = frame[-1]
    if (last & 0xF0) != 0xF0:
        return None
    # Long format: '72 CRC1 CRC2 <EOF>' at the end
    if len(frame) >= 7 and frame[-4] == 0x72:
        crc = frame[-3:-1]
        data = frame[3:-4]
        if data and data[-1] == 0x03:
            data = data[:-1]
        return bytes(data), bytes(crc)
    # Short format
    data = frame[3:-1]
    if data and data[-1] == 0x03:
        data = data[:-1]
    return bytes(data), None


def decode_data(data: bytes) -> str:
    """Decode data byte stream to ASCII string via inverse complement."""
    return ''.join(chr(0x7F - (b & 0x7F)) for b in data)


def parse_value(decoded: str):
    """Convert decoded ASCII text into a typed Python value.

    Rules:
      - '@' (single char) → None (slave reported invalid/empty)
      - Trailing space (0x20) → integer
      - Trailing scale-char (0x21..0x2F) → float with decimal_places = ord(last) - 0x20
        e.g. '00!' → 0.0 (dp=1), '21166$' → 2.1166 (dp=4)
      - Otherwise → return decoded as string (TAGs, identifiers)
    """
    if not decoded:
        return None
    if decoded == '@':
        return None
    last = decoded[-1]
    body = decoded[:-1]
    if last == ' ':
        try:
            return int(body)
        except ValueError:
            return body
    if 0x21 <= ord(last) <= 0x2F:
        dp = ord(last) - 0x20
        sign = 1
        if body.startswith('-'):
            sign = -1
            body = body[1:]
        try:
            return sign * int(body) / (10 ** dp)
        except ValueError:
            return decoded
    return decoded
