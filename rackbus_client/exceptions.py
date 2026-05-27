"""
Custom exceptions for the Rackbus client.
"""


class RackbusError(Exception):
    """Base class for all Rackbus client errors."""


class RackbusTimeoutError(RackbusError):
    """Slave did not respond with usable data within the retry budget."""


class RackbusChecksumError(RackbusError):
    """A received frame had a checksum that did not match the body."""


class RackbusDecodeError(RackbusError):
    """A received frame could not be decoded (malformed, unknown format, etc.)."""
