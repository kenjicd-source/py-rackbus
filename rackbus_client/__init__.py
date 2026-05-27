"""
py-rackbus — Python client for Endress+Hauser Rackbus SL protocol.

Public API:
    from rackbus_client import RackbusClient
    c = RackbusClient(port='COM5', address=3)
    c.read_mass_flow()
    c.close()
"""

from .client import RackbusClient
from .exceptions import (
    RackbusChecksumError,
    RackbusDecodeError,
    RackbusError,
    RackbusTimeoutError,
)
from .parameters import PARAMETERS, VHCode

__all__ = [
    "RackbusClient",
    "RackbusError",
    "RackbusTimeoutError",
    "RackbusChecksumError",
    "RackbusDecodeError",
    "PARAMETERS",
    "VHCode",
]

__version__ = "0.1.0"
