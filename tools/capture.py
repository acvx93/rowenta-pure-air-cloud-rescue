"""Record and decode the UART (Gizwits protocol) between the purifier board and the WiFi module.

Logic analyzer wiring: CH1 (D0) = module TX, CH2 (D1) = module RX (what the board sends).
Needs sigrok-cli: set SIGROK_CLI to its path if it is not on PATH.

Usage:  python capture.py record <seconds> <name>   (record, then decode)
        python capture.py show <name>               (decode an existing recording)
"""
import os
import re
import shutil
import subprocess
import sys

from gizwits import describe, frames

HERE = os.path.dirname(os.path.abspath(__file__))
CAPTURES = os.path.join(HERE, "..", "captures")
SIGROK = (os.environ.get("SIGROK_CLI") or shutil.which("sigrok-cli")
          or r"C:\Program Files\sigrok\sigrok-cli\sigrok-cli.exe")
SAMPLERATE = 1_000_000
BAUD = 9600
CHANNELS = {"D0": "module->board", "D1": "board->module"}


def record(seconds, path):
    subprocess.run([SIGROK, "-d", "fx2lafw", "--config", f"samplerate={SAMPLERATE}",
                    "--time", f"{seconds}s", "--channels", ",".join(CHANNELS), "-o", path], check=True)


def parse_samplerate(show_output):
    """Sample rate from `sigrok-cli --show` output (recordings may not use SAMPLERATE)."""
    m = re.search(r"^Samplerate: (\d+)", show_output, re.M)
    return int(m.group(1)) if m else SAMPLERATE


def uart_bytes(path, channel):
    show = subprocess.run([SIGROK, "-i", path, "--show"], capture_output=True, text=True).stdout
    rate = parse_samplerate(show)
    out = subprocess.run([SIGROK, "-i", path, "-P", f"uart:rx={channel}:baudrate={BAUD}",
                          "-A", "uart=rx-data", "--protocol-decoder-samplenum"],
                         capture_output=True, text=True).stdout
    for m in re.finditer(r"^(\d+)-\d+ \S+: ([0-9A-F]{2})$", out, re.M):
        yield int(m.group(1)) / rate, int(m.group(2), 16)


def show(path):
    events = []
    for channel, direction in CHANNELS.items():
        for t, frame in frames(uart_bytes(path, channel)):
            events.append((t, direction, frame))
    last_status = None
    for t, direction, frame in sorted(events):
        text, cmd, payload = describe(frame)
        print(f"{t:8.3f}s {direction:13} {text}")
        if cmd in (0x04, 0x05) and len(payload) > 1:
            status = payload[1:]
            print(" " * 24 + " ".join(f"{b:02X}" for b in status))
            if last_status and len(last_status) == len(status) and status != last_status:
                changes = [f"byte {i}: {a:02X}->{b:02X}" for i, (a, b) in enumerate(zip(last_status, status)) if a != b]
                print(" " * 24 + "CHANGES: " + ", ".join(changes))
            last_status = status
        elif len(payload):
            print(" " * 24 + payload.hex(" ").upper())


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("record", "show"):
        sys.exit(__doc__)
    os.makedirs(CAPTURES, exist_ok=True)
    if sys.argv[1] == "record":
        seconds, name = int(sys.argv[2]), sys.argv[3]
        path = os.path.join(CAPTURES, name + ".sr")
        print(f"Recording {seconds}s to {path}...")
        record(seconds, path)
    else:
        path = os.path.join(CAPTURES, sys.argv[2] + ".sr")
    show(path)


if __name__ == "__main__":
    main()
