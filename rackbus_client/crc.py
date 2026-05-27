"""
Rackbus SL custom checksum (CRC).

REQUEST CRC formula (reverse-engineered, verified on 110 transactions):
    crc1 = 0x2E XOR (0x08 if odd count of MSB-set bytes in body else 0)
    crc2 = (sum(byte & 0x7F for byte in body) + 0x48) mod 256

`body` is everything from SOF (0xCA) up to and including the
end-of-payload marker (0x72) — NOT including the CRC bytes
themselves or the EOF (0xF2).

Both CRC bytes are then parity-augmented (MSB = even parity) before
being placed on the wire.

RESPONSE CRC formula: not fully reverse-engineered. We do not currently
verify response CRC — the slave (Promass) is trusted and decoding the
data section is sufficient for our use case. A `verify_response_crc()`
stub is included so that callers can opt into verification when the
formula is found.
"""
from __future__ import annotations

from typing import Tuple


def compute_request_crc(body: bytes) -> Tuple[int, int]:
    """Compute the (crc1, crc2) pair for a request body — RAW (without parity)."""
    msb_count = sum(1 for b in body if b & 0x80)
    crc1 = 0x2E ^ (0x08 if msb_count % 2 else 0x00)
    crc2 = (sum(b & 0x7F for b in body) + 0x48) & 0xFF
    return crc1, crc2


def verify_request_crc(body_with_crc: bytes) -> bool:
    """Verify the CRC of a request body that includes the (raw, parity-stripped)
    CRC bytes appended at the end.
    """
    if len(body_with_crc) < 3:
        return False
    body = body_with_crc[:-2]
    crc1, crc2 = compute_request_crc(body)
    return (body_with_crc[-2] & 0x7F) == crc1 and (body_with_crc[-1] & 0x7F) == crc2


def verify_response_crc(body_with_crc: bytes) -> bool:
    """Stub — response CRC formula not yet reverse-engineered.

    Always returns True. When the formula is recovered, replace this
    body with the real verification.
    """
    return True
