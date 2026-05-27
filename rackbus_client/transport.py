"""
Low-level transport for Rackbus SL: serial port + multi-stage exchange pattern.

The Rackbus SL protocol is NOT a simple request/response. The slave (Promass)
demands a sequence of *continuation* frames from master before it sends data.

Reverse-engineered behavior:

  1. If the bus has been idle for more than ~1-2 seconds, the slave needs
     to be "woken up" with several presence-poll frames `CA 3F F2`.
  2. A single read transaction then looks like:
         TX  VR-request   (12 bytes)
         wait 50 ms
         TX  CA DD AA 28 F2     (continuation 1)
         wait 50 ms
         TX  CA 48 F2           (continuation 2)
         listen 500 ms
  3. The slave's response sequence: ack, ack-or-empty-data, data frame.
  4. First attempt sometimes returns an empty data frame ("not ready yet").
     Retrying after another warmup usually yields the data.
"""

from __future__ import annotations

import logging
import time

import serial

from .codec import (
    decode_data,
    encode_vr_request,
    parse_response_frame,
    parse_value,
    split_frames,
)
from .exceptions import RackbusTimeoutError

log = logging.getLogger(__name__)


# Continuation patterns reverse-engineered from ZA672 wire dump
POLL = bytes.fromhex("CA3FF2")  # master presence-poll / slave ack
CONT_DD = bytes.fromhex("CADDAA28F2")  # continuation 1 ("ack polling")
CONT_48 = bytes.fromhex("CA48F2")  # continuation 2 ("send data")


class RackbusTransport:
    """RS-485 transport implementing the Rackbus SL multi-stage exchange."""

    def __init__(
        self,
        port: str,
        baudrate: int = 19200,
        bytesize: int = 8,
        parity: str = "N",
        stopbits: int = 1,
        timeout: float = 0.05,
        inter_chunk_ms: int = 50,
        listen_timeout: float = 0.5,
        warmup_count: int = 5,
        retries: int = 5,
    ):
        log.info("Opening Rackbus port %s @ %d %d%s%d", port, baudrate, bytesize, parity, stopbits)
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=bytesize,
            parity=parity,
            stopbits=stopbits,
            timeout=timeout,
        )
        self.inter_chunk_ms = inter_chunk_ms
        self.listen_timeout = listen_timeout
        self.warmup_count = warmup_count
        self.retries = retries

    def close(self) -> None:
        if self.ser.is_open:
            self.ser.close()
            log.info("Rackbus port closed")

    def __enter__(self) -> RackbusTransport:
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    # --- internal ---

    def _warmup(self) -> None:
        """Send presence-polls to wake the slave from idle."""
        for _ in range(self.warmup_count):
            self.ser.write(POLL)
            self.ser.flush()
            time.sleep(0.05)
            self.ser.read(64)  # drain any acks
        time.sleep(0.2)

    def _listen(self, total_s: float | None = None) -> bytes:
        """Read until inter-frame silence > 100 ms or total timeout."""
        if total_s is None:
            total_s = self.listen_timeout
        rx = bytearray()
        end = time.time() + total_s
        last_rx = None
        while time.time() < end:
            data = self.ser.read(512)
            now = time.time()
            if data:
                rx.extend(data)
                last_rx = now
            elif last_rx and (now - last_rx) > 0.1:
                break
            else:
                time.sleep(0.005)
        return bytes(rx)

    # --- public ---

    def read_variable(self, addr: int, v: int, h: int):
        """Execute a Variable Read transaction.

        Returns (value, decoded_text, raw_data_bytes) or raises
        RackbusTimeoutError if the slave gives only empty/no responses.
        """
        frame = encode_vr_request(addr, v, h)
        gap = self.inter_chunk_ms / 1000.0

        last_rx = b""
        for attempt in range(self.retries):
            self._warmup()
            log.debug("[attempt %d] TX VR: %s", attempt + 1, frame.hex(" "))

            self.ser.write(frame)
            self.ser.flush()
            time.sleep(gap)
            self.ser.write(CONT_DD)
            self.ser.flush()
            time.sleep(gap)
            self.ser.write(CONT_48)
            self.ser.flush()

            last_rx = self._listen()
            log.debug("RX (%dB): %s", len(last_rx), last_rx.hex(" "))

            for f in split_frames(last_rx):
                if len(f) >= 4 and f[0] == 0xCA and f[1] == 0xC0:
                    parsed = parse_response_frame(f)
                    if parsed is None:
                        continue
                    data, crc = parsed
                    if data:  # non-empty payload
                        decoded = decode_data(data)
                        value = parse_value(decoded)
                        log.info("VR %d,%d,%d → %r", addr, v, h, value)
                        return value, decoded, data

        raise RackbusTimeoutError(
            f"No data from slave at addr={addr}, V={v}, H={h} after "
            f"{self.retries} attempts. Last RX: {last_rx.hex(' ')}"
        )

    def listen_passive(self, duration_s: float = 5.0) -> bytes:
        """Passively listen on the bus (for diagnostics)."""
        log.info("Passive listen for %.1f s", duration_s)
        return self._listen(total_s=duration_s)
