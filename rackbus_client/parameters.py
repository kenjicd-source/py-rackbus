"""
V/H parameter map for Endress+Hauser Promass 63 (Evaluation Mode 1).

V/H positions are derived from device behavior cross-checked against
the publicly available operating manual. Each entry pairs a symbolic
name with the (V, H) coordinates the slave responds to.

Confirmed working parameters (read from a real Promass 63 SW 3.00.00) are
marked in the description.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class VHCode:
    v: int
    h: int
    name: str
    unit: str = ""
    kind: str = "float"  # 'float' | 'int' | 'string'
    description: str = ""


PARAMETERS: list = [
    # V0 — process variables
    VHCode(0, 0, "MASS_FLOW", "t/h", "float", "Mass flow rate"),
    VHCode(0, 1, "TOTALIZER_1", "t", "float", "Totalizer 1"),
    VHCode(0, 2, "TOTALIZER_1_OVF", "count", "int", "Totalizer 1 overflow counter"),
    VHCode(0, 3, "TOTALIZER_2", "t", "float", "Totalizer 2"),
    VHCode(0, 4, "TOTALIZER_2_OVF", "count", "int", "Totalizer 2 overflow counter"),
    VHCode(0, 5, "VOLUME_FLOW", "m3/h", "float", "Volume flow rate"),
    VHCode(0, 6, "STD_VOLUME_FLOW", "m3/h", "float", "Standard volume flow rate"),
    # V1 — secondary measurements
    VHCode(1, 0, "DENSITY", "g/cm3", "float", "Density"),
    VHCode(1, 1, "TEMPERATURE", "C", "float", "Process temperature"),
    VHCode(1, 2, "CALC_DENSITY", "g/cm3", "float", "Calculated density"),
    # V2 — communication / system
    VHCode(2, 0, "EVALUATION_MODE", "", "int", "0=matrix 1, 1=matrix 2"),
    VHCode(2, 1, "ACCESS_CODE", "", "int", "Access code"),
    VHCode(2, 2, "DIAGNOSTIC_CODE", "", "int", "Active diagnostic code"),
    VHCode(2, 3, "INTERFACE", "", "int", "Comm interface selector"),
    VHCode(2, 4, "BUS_ADDRESS", "", "int", "Rackbus address (0..63)"),
    VHCode(2, 5, "SYSTEM_CONFIG", "", "int", "System config selector"),
    VHCode(2, 6, "SW_VERSION_COM", "", "int", "Comm module SW version"),
    # V3 — units
    VHCode(3, 0, "MASS_FLOW_UNIT", "", "int", "Mass flow unit code"),
    VHCode(3, 1, "MASS_UNIT", "", "int", "Mass unit code"),
    VHCode(3, 2, "FLOWRATE_UNITS", "", "int", "Flowrate units"),
    VHCode(3, 3, "VOLUME_UNITS", "", "int", "Volume units"),
    VHCode(3, 4, "GALLON_BARREL", "", "int", "Gallon/barrel selector"),
    VHCode(3, 7, "PIPE_SIZE_UNIT", "", "int", "Pipe size unit"),
    # V4 — totalizer & display config (mode 1)
    VHCode(4, 0, "RESET_TOTALIZER", "", "int", "0:cancel, 1:rst1, 2:rst2, 3:both"),
    VHCode(4, 1, "ASSIGN_TOTAL_1", "", "int", "What variable totalizer 1 sums"),
    VHCode(4, 2, "ASSIGN_TOTAL_2", "", "int", "What variable totalizer 2 sums"),
    VHCode(4, 3, "LCD_CONTRAST", "", "int", "LCD contrast"),
    VHCode(4, 4, "LANGUAGE", "", "int", "Display language"),
    VHCode(4, 5, "DISPLAY_DAMPING", "", "int", "Display damping"),
    VHCode(4, 6, "DISPLAY_LINE_1", "", "int", "Display line 1 content"),
    VHCode(4, 7, "DISPLAY_LINE_2", "", "int", "Display line 2 content"),
    VHCode(4, 8, "FORMAT_FLOW", "", "int", "Flow format"),
    # V5 — current output 4-20 mA
    VHCode(5, 0, "ASSIGN_OUTPUT", "", "int", "Output variable assignment"),
    VHCode(5, 1, "VALUE_FOR_0_4_MA", "", "float", "Value at 0/4 mA"),
    VHCode(5, 2, "FULL_SCALE_1", "", "float", "Full scale 1"),
    # V6 — pulse/freq output
    VHCode(6, 0, "ASSIGN_PULS_FREQ", "", "int", "Pulse/freq variable"),
    VHCode(6, 3, "PULSE_WIDTH", "", "float", "Pulse width"),
    # V7 — process configuration
    VHCode(7, 0, "LOW_FLOW_CUTOFF", "t/h", "float", "Low-flow cutoff"),
    VHCode(7, 1, "EPD_ASSIGN", "", "int", "EPD function"),
    VHCode(7, 4, "EPD_THRESHOLD", "g/cm3", "float", "Empty pipe detection threshold"),
    VHCode(7, 5, "EPD_RESPONSE", "", "int", "EPD response code"),
    VHCode(7, 6, "EPD_ALARM", "", "int", "EPD alarm enable"),
    # V8 — amplifier signal processing
    VHCode(8, 5, "AMP_SW_VERSION", "", "int", "Amplifier software version"),
    # V9 — sensor / calibration
    VHCode(9, 0, "CALIBR_FACTOR", "", "float", "Calibration K-factor"),
    VHCode(9, 1, "ZERO_POINT", "", "int", "Zero point offset"),
    VHCode(9, 2, "NOMINAL_SIZE", "mm", "float", "Pipe nominal size DN"),
    VHCode(9, 5, "SERIAL_NUMBER", "", "int", "Sensor serial number"),
    # V10 (A) — identification
    VHCode(10, 0, "TAG_NUMBER", "", "string", "User-assigned tag (text)"),
]


BY_NAME: dict = {p.name: p for p in PARAMETERS}
BY_VH: dict = {(p.v, p.h): p for p in PARAMETERS}


def get_by_name(name: str) -> VHCode:
    if name not in BY_NAME:
        raise KeyError(f"Unknown parameter name: {name!r}")
    return BY_NAME[name]


def get_by_vh(v: int, h: int) -> VHCode | None:
    return BY_VH.get((v, h))


def parse_vh_code(code: str):
    """Parse 'V0H0' or 'V10H5' into (v, h) tuple."""
    import re

    m = re.fullmatch(r"V(\d+)H(\d+)", code)
    if not m:
        raise ValueError(f"Bad V/H code: {code!r} (expected like 'V0H0')")
    return int(m.group(1)), int(m.group(2))
