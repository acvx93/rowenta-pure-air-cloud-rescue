"""Talk to the purifier board through the ESP32 bridge (firmware-bridge/).

Set BRIDGE_PORT to the ESP32's serial port (default COM4).

Usage:  python bridge.py listen [seconds]
        python bridge.py send <cmd hex> <payload hex | -> [seconds]   e.g. send 03 02, send 07 -
"""
import os
import sys
import time

import serial

from gizwits import build_frame, describe, frames

PORT = os.environ.get("BRIDGE_PORT", "COM4")


def open_bridge():
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = PORT, 115200, 0.1
    s.dtr = s.rts = False  # don't reset the ESP32 when opening the port
    s.open()
    return s


def timed_bytes(s, seconds):
    start = time.time()
    while time.time() - start < seconds:
        for b in s.read(256):
            yield time.time() - start, b


def show(s, seconds, ack=True):
    """Print the board's frames and, like the original module, acknowledge its reports."""
    for t, frame in frames(timed_bytes(s, seconds)):
        text, cmd, payload = describe(frame)
        print(f"{t:7.2f}s board: {text}")
        if payload:
            print(" " * 15 + payload.hex(" ").upper())
        if ack and cmd in (0x05, 0x09, 0x17):
            s.write(build_frame(cmd + 1, frame[5]))


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("listen", "send"):
        sys.exit(__doc__)
    s = open_bridge()
    if sys.argv[1] == "listen":
        show(s, float(sys.argv[2]) if len(sys.argv) > 2 else 15)
    else:
        payload = b"" if sys.argv[3] == "-" else bytes.fromhex(sys.argv[3])
        frame = build_frame(int(sys.argv[2], 16), 0x01, payload)
        print("sending:", frame.hex(" ").upper())
        s.write(frame)
        show(s, float(sys.argv[4]) if len(sys.argv) > 4 else 5)


if __name__ == "__main__":
    main()
