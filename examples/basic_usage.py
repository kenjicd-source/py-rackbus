#!/usr/bin/env python3
"""
Basic usage example for the Rackbus SL client.

Connect directly to a Promass 63 (or compatible Rackbus SL slave) via
an RS-485 adapter — no proprietary gateway needed.

Wiring (standard RS-485):
    Promass term 20 (Data A) → USB-RS485 A (D+)
    Promass term 21 (Data B) → USB-RS485 B (D-)
    Promass term 28 (shield) → USB-RS485 GND (optional)
    Promass term 1 / 2:        220 VAC power

Usage:
    python examples/basic_usage.py [COM_PORT] [BUS_ADDRESS]

If the package is not installed, run from the repo root so the local
`rackbus_client/` is importable.
"""

import logging
import sys
from pathlib import Path

# Add repo root to sys.path so `rackbus_client` works without installation.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from rackbus_client import RackbusClient


def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    address = int(sys.argv[2]) if len(sys.argv) > 2 else 3

    with RackbusClient(port=port, address=address) as client:
        print(f"\nReading Promass at addr={address} via {port}\n")
        print(f"Mass flow:     {client.read_mass_flow()} t/h")
        print(f"Density:       {client.read_density()} g/cm3")
        print(f"Temperature:   {client.read_temperature()} C")
        print(f"Totalizer 1:   {client.read_totalizer_1()} t")
        print(f"TAG:           {client.read_tag_number()!r}")
        print(f"K-factor:      {client.read_calibration_factor()}")
        print(f"Bus address:   {client.read_bus_address()}")

        print("\n=== Bulk read ===")
        for name, value in client.read_all().items():
            print(f"  {name:<22s} = {value!r}")


if __name__ == "__main__":
    main()
