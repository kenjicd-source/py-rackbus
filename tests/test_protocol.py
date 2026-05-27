"""
Regression tests for the Rackbus SL protocol layer.

Each test case is a known wire pair (from a real Promass 63), so any
break in encoder / decoder / CRC will be caught.
"""

import json
from pathlib import Path

import pytest

from rackbus_client.codec import (
    char_to_wire,
    decode_data,
    encode_vr_request,
    even_parity_bit,
    parse_response_frame,
    parse_value,
    split_frames,
    wire_to_char,
    with_parity,
)
from rackbus_client.crc import compute_request_crc, verify_request_crc
from rackbus_client.parameters import (
    PARAMETERS,
    get_by_name,
    get_by_vh,
    parse_vh_code,
)

FIXTURES_FILE = Path(__file__).resolve().parent / "test_fixtures.json"


# === Parity ===


@pytest.mark.parametrize(
    "byte, expected_parity",
    [
        (0x00, 0),
        (0x01, 1),
        (0x33, 0),  # '3' lower-7 = 4 ones, even
        (0x52, 1),  # 'R' = 3 ones, odd
        (0x5F, 0),  # '_' = 6 ones, even
        (0x72, 0),  # 'r' = 4 ones, even
    ],
)
def test_even_parity_bit(byte, expected_parity):
    assert even_parity_bit(byte) == expected_parity


@pytest.mark.parametrize(
    "byte_in, wire_out",
    [
        (0x33, 0x33),  # '3'
        (0x52, 0xD2),  # 'R'
        (0x2C, 0xAC),  # ','
        (0x0D, 0x8D),  # CR
    ],
)
def test_with_parity(byte_in, wire_out):
    assert with_parity(byte_in) == wire_out


# === DATA char ↔ wire (0x7F base, used in responses) ===


@pytest.mark.parametrize(
    "char, wire",
    [
        ("0", 0xCF),  # 0x7F-0x30=0x4F + parity 1
        ("1", 0x4E),
        ("2", 0x4D),
        ("3", 0xCC),
        ("5", 0xCA),
        ("9", 0xC6),
        ("-", 0xD2),
    ],
)
def test_char_to_wire_data(char, wire):
    assert char_to_wire(char) == wire


@pytest.mark.parametrize(
    "wire, char",
    [
        (0xCF, "0"),
        (0x4E, "1"),
        (0x4D, "2"),
        (0xCC, "3"),
        (0xCA, "5"),
        (0xC6, "9"),
        (0xD2, "-"),
    ],
)
def test_wire_to_char(wire, char):
    assert wire_to_char(wire) == char


# === VR request encoding (10 verified pairs from ZA672 sniff) ===


@pytest.mark.parametrize(
    "addr,v,h,expected_hex",
    [
        (3, 0, 0, "CA225FAC5F5F5F5F722E2DF2"),
        (3, 0, 1, "CA225FAC5F5F5FDE72A6ACF2"),
        (3, 0, 2, "CA225FAC5F5F5FDD72A62BF2"),
        (3, 0, 3, "CA225FAC5F5F5F5C722EAAF2"),
        (3, 0, 4, "CA225FAC5F5F5FDB72A6A9F2"),
        (3, 1, 0, "CA225FAC5FDE5F5F72A6ACF2"),
        (3, 1, 1, "CA225FAC5FDE5FDE722E2BF2"),
        (3, 2, 4, "CA225FAC5FDD5FDB722E27F2"),
        (3, 9, 0, "CA225FAC5F565F5F722E24F2"),
        (3, 10, 0, "CA225FAC5F555F5F722EA3F2"),
    ],
)
def test_encode_vr_byte_for_byte(addr, v, h, expected_hex):
    expected = bytes.fromhex(expected_hex)
    assert (
        encode_vr_request(addr, v, h) == expected
    ), f"VR {addr},{v},{h}: expected {expected_hex}, got {encode_vr_request(addr,v,h).hex().upper()}"


def test_frame_structure():
    frame = encode_vr_request(3, 0, 0)
    assert frame[0] == 0xCA  # SOF
    assert frame[1] == 0x22  # FUN '"' (parity 0)
    assert frame[2] == 0x5F  # SEP
    assert frame[-4] == 0x72  # EOP 'r' (parity 0)
    assert frame[-1] == 0xF2  # EOM


# === CRC for requests ===


def test_request_crc_v0h0():
    frame = encode_vr_request(3, 0, 0)
    body = frame[:-3]
    assert compute_request_crc(body) == (0x2E, 0x2D)


def test_request_crc_v0h1():
    """CRC1 flips bit 3 when MSB-set bytes parity flips."""
    body = encode_vr_request(3, 0, 1)[:-3]
    assert compute_request_crc(body) == (0x26, 0x2C)


def test_request_crc_verify_with_raw_crc():
    frame = encode_vr_request(3, 0, 1)
    raw_body_with_raw_crc = frame[:-3] + bytes([frame[-3] & 0x7F, frame[-2] & 0x7F])
    assert verify_request_crc(raw_body_with_raw_crc)


# === Response parsing ===


def test_parse_long_response_with_data():
    frame = bytes.fromhex("CAC05FCFCFDE03722E21F2")
    data, crc = parse_response_frame(frame)
    assert data == bytes.fromhex("CFCFDE")
    assert crc == bytes.fromhex("2E21")


def test_parse_empty_response():
    frame = bytes.fromhex("CAC05F72A52EF2")
    data, crc = parse_response_frame(frame)
    assert data == b""
    assert crc == bytes.fromhex("A52E")


def test_reject_non_response():
    req = bytes.fromhex("CA225FAC5F5F5F5F722E2DF2")
    assert parse_response_frame(req) is None


# === Data decoding ===


@pytest.mark.parametrize(
    "hex_in, decoded, value",
    [
        ("CFCFDE", "00!", 0.0),
        ("CADE", "5!", 0.5),
        ("4DCCCAC6DD", '2359"', 23.59),
        ("4D4E4EC9C9DB", "21166$", 2.1166),
        ("CC5F", "3 ", 3),
        ("D24E4E485F", "-117 ", -117),
        ("3F", "@", None),
    ],
)
def test_decode_and_parse(hex_in, decoded, value):
    data = bytes.fromhex(hex_in)
    assert decode_data(data) == decoded
    result = parse_value(decoded)
    if isinstance(value, float):
        assert abs(result - value) < 1e-9
    else:
        assert result == value


def test_decode_tag_string():
    # Synthetic payload computed from the standard 0x7F-complement+parity
    # encoding rule for 'DEMO TAG' (8 ASCII chars).
    data = bytes.fromhex("BB3AB2305F2BBEB8")
    assert decode_data(data) == "DEMO TAG"
    assert parse_value("DEMO TAG") == "DEMO TAG"


# === split_frames ===


def test_split_frames_multi_eof():
    """Both F2, FA, FF act as EOM."""
    data = bytes.fromhex("CA3FF2CAC05FCFCFDEFFCAC05F72A52EFA")
    frames = split_frames(data)
    assert len(frames) == 3
    assert frames[0].endswith(b"\xf2")
    assert frames[1].endswith(b"\xff")
    assert frames[2].endswith(b"\xfa")


# === Parameter table ===


def test_parameters_unique():
    seen_vh, seen_name = set(), set()
    for p in PARAMETERS:
        assert (p.v, p.h) not in seen_vh, f"Duplicate V/H: {p}"
        assert p.name not in seen_name, f"Duplicate name: {p}"
        seen_vh.add((p.v, p.h))
        seen_name.add(p.name)


def test_get_by_name():
    assert get_by_name("MASS_FLOW").v == 0


def test_get_by_vh():
    assert get_by_vh(2, 4).name == "BUS_ADDRESS"


def test_parse_vh_code():
    assert parse_vh_code("V0H0") == (0, 0)
    assert parse_vh_code("V10H5") == (10, 5)
    with pytest.raises(ValueError):
        parse_vh_code("garbage")


# === Fixture-driven roundtrip ===


@pytest.fixture(scope="module")
def fixtures():
    with FIXTURES_FILE.open(encoding="utf-8") as f:
        return json.load(f)["fixtures"]


def test_fixtures_load(fixtures):
    assert len(fixtures) >= 8


def test_all_fixtures_tx_match(fixtures):
    """For every fixture, our encoder reproduces the captured TX byte-for-byte."""
    for fx in fixtures:
        addr = fx["address"]
        v, h = parse_vh_code(fx["parameter"])
        our_tx = encode_vr_request(addr, v, h)
        captured_tx = bytes.fromhex(fx["tx_frame"].replace(" ", ""))
        assert our_tx == captured_tx, (
            f"{fx['name']}: encoder mismatch.\n"
            f"  expected: {captured_tx.hex(' ')}\n"
            f"  got:      {our_tx.hex(' ')}"
        )


def test_all_fixtures_decode(fixtures):
    """Every fixture's payload decodes back to the expected value."""
    for fx in fixtures:
        if fx["rx_data_frame"] is None:
            continue
        frame = bytes.fromhex(fx["rx_data_frame"].replace(" ", ""))
        data, _crc = parse_response_frame(frame)
        decoded = decode_data(data)
        value = parse_value(decoded)
        expected = fx["expected_value"]
        if isinstance(expected, float):
            assert (
                value is not None and abs(value - expected) < 1e-6
            ), f"{fx['name']}: expected {expected}, got {value}"
        else:
            assert value == expected, f"{fx['name']}: expected {expected!r}, got {value!r}"
