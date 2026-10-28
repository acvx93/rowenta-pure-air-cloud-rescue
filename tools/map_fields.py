"""Find which bit of the write field mask controls each status field.

Sends full write commands (action 01 + 3-byte mask + VALS_LEN values) with a single mask bit
set and every value set to FILL (never 0, so the mode is never set to 0).

WARNING: this writes to the board and can leave it in a bad state; read docs/protocol.md first.

Usage:  python map_fields.py [fill hex]
"""
import sys

from bridge import open_bridge
from gizwits import build_frame
from write_probe import exchange, read_status

VALS_LEN = 40


def main():
    fill = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x02
    s = open_bridge()
    exchange(s, build_frame(0x07, 0x01), 0.5)
    before = read_status(s)
    print("initial status:", before.hex(" ").upper())
    for bit in range(24):
        flags = (1 << bit).to_bytes(3, "big")
        payload = b"\x01" + flags + bytes([fill]) * VALS_LEN
        exchange(s, build_frame(0x03, 0x10 + bit, payload))
        after = read_status(s)
        if after is None:
            print(f"bit {bit:2}: no reply to the read")
            continue
        changes = [f"b{i}:{a:02X}->{b:02X}" for i, (a, b) in enumerate(zip(before, after)) if a != b and i != 3]
        print(f"bit {bit:2} (mask {flags.hex(' ').upper()}): {', '.join(changes) or '-'}")
        before = after
    print("final status:  ", before.hex(" ").upper())


if __name__ == "__main__":
    main()
