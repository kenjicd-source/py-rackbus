"""
High-level Rackbus SL client.

Example:
    from rackbus_client import RackbusClient

    with RackbusClient(port='COM5', address=3) as c:
        print(c.read_mass_flow())
        print(c.read_all())
"""

from __future__ import annotations

import logging
from typing import Dict, List, Optional, Union

from .exceptions import RackbusTimeoutError
from .parameters import (
    get_by_name,
    parse_vh_code,
)
from .transport import RackbusTransport

log = logging.getLogger(__name__)

Value = Union[float, int, str, None]


class RackbusClient:
    """High-level client for any Rackbus SL slave (tested on Promass 63).

    Args:
        port: serial port (e.g. 'COM5' on Windows, '/dev/ttyUSB0' on Linux).
        address: Rackbus slave address (0..63).
        baud: baud rate. Standard Rackbus SL is 19200.
        **kwargs: passed through to :class:`RackbusTransport`.

    Example:
        >>> with RackbusClient('COM5', address=3) as c:
        ...     mass_flow = c.read_mass_flow()
        ...     density = c.read_density()
        ...     data = c.read_all()
    """

    def __init__(self, port: str, address: int = 3, baud: int = 19200, **kwargs):
        self.address = address
        self.transport = RackbusTransport(port, baudrate=baud, **kwargs)

    def __enter__(self) -> RackbusClient:
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def close(self) -> None:
        """Close the underlying serial port."""
        self.transport.close()

    # --- Generic read ---

    def read_parameter(self, name_or_code: str) -> Value:
        """Read a parameter by name ('MASS_FLOW') or V/H code ('V0H0').

        Returns a typed value (float/int/str) or None on invalid/empty.
        """
        if name_or_code.startswith("V") and "H" in name_or_code:
            v, h = parse_vh_code(name_or_code)
        else:
            param = get_by_name(name_or_code)
            v, h = param.v, param.h

        try:
            value, _decoded, _raw = self.transport.read_variable(self.address, v, h)
        except RackbusTimeoutError:
            log.warning("Timeout reading %s", name_or_code)
            return None
        return value

    def read_parameter_raw(self, v: int, h: int):
        """Read by raw V/H coordinates. Returns (value, decoded_text, raw_bytes)."""
        return self.transport.read_variable(self.address, v, h)

    # --- Convenience accessors ---

    def read_mass_flow(self) -> Optional[float]:
        """Mass flow in t/h."""
        return self.read_parameter("MASS_FLOW")

    def read_density(self) -> Optional[float]:
        """Density in g/cm³."""
        return self.read_parameter("DENSITY")

    def read_temperature(self) -> Optional[float]:
        """Process temperature in °C."""
        return self.read_parameter("TEMPERATURE")

    def read_totalizer_1(self) -> Optional[float]:
        """Totalizer 1 in t."""
        return self.read_parameter("TOTALIZER_1")

    def read_totalizer_2(self) -> Optional[float]:
        """Totalizer 2 in t."""
        return self.read_parameter("TOTALIZER_2")

    # Alias requested in the API design
    read_totalizer = read_totalizer_1

    def read_volume_flow(self) -> Optional[float]:
        """Volume flow in m³/h."""
        return self.read_parameter("VOLUME_FLOW")

    def read_tag_number(self) -> Optional[str]:
        """User-assigned tag text."""
        return self.read_parameter("TAG_NUMBER")

    def read_bus_address(self) -> Optional[int]:
        """Slave's own knowledge of its bus address (sanity check)."""
        return self.read_parameter("BUS_ADDRESS")

    def read_calibration_factor(self) -> Optional[float]:
        """K-factor (calibration)."""
        return self.read_parameter("CALIBR_FACTOR")

    def read_serial_number(self) -> Optional[int]:
        """Sensor serial number."""
        return self.read_parameter("SERIAL_NUMBER")

    # --- Bulk ---

    def read_all(self, names: Optional[List[str]] = None) -> Dict[str, Value]:
        """Read a list of parameters (default: a useful subset). Returns dict."""
        if names is None:
            names = [
                "MASS_FLOW",
                "TOTALIZER_1",
                "VOLUME_FLOW",
                "DENSITY",
                "TEMPERATURE",
                "BUS_ADDRESS",
                "CALIBR_FACTOR",
                "ZERO_POINT",
                "NOMINAL_SIZE",
                "SERIAL_NUMBER",
                "TAG_NUMBER",
            ]
        out: Dict[str, Value] = {}
        for n in names:
            try:
                out[n] = self.read_parameter(n)
            except Exception as e:  # pragma: no cover
                log.error("Error reading %s: %s", n, e)
                out[n] = None
        return out
