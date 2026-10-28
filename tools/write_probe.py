"""Send Gizwits write variants (cmd 03, action 01) and show which status bytes change.

WARNING: experimental writes can leave the board in a bad state (motor stopped, high-pitched
whine, surviving a power cycle). Read docs/protocol.md first: always send full-length payloads
and never set mode 0.

Usage:  python write_probe.py <payload hex> [<payload hex> ...]
"""
import sys

from bridge import open_bridge, timed_bytes
from gizwits import build_frame, frames


def exchange(s, frame, seconds=1.5):
    """Send a frame and return the frames received (acknowledging reports like the module)."""
    s.reset_input_buffer()
    s.write(frame)
    got = []
    for _, f in frames(timed_bytes(s, seconds)):
        got.append(f)
        if f[4] == 0x05:
            s.write(build_frame(0x06, f[5]))
    return got


def read_status(s):
    for f in exchange(s, build_frame(0x03, 0x01, b"\x02")):
        if f[4] == 0x04 and f[8] == 0x03:
            return f[9:-1]
    return None


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    s = open_bridge()
    exchange(s, build_frame(0x07, 0x01), 0.5)  # heartbeat, so the board treats us as connected
    before = read_status(s)
    print("initial status:", before.hex(" ").upper())
    for sn, payload_hex in enumerate(sys.argv[1:], start=2):
        payload = bytes.fromhex(payload_hex)
        replies = exchange(s, build_frame(0x03, sn, payload))
        after = read_status(s)
        answer = ", ".join(f"cmd {f[4]:02X} {f[8:-1].hex(' ').upper()}".strip() for f in replies) or "no reply"
        changes = [f"b{i}:{a:02X}->{b:02X}" for i, (a, b) in enumerate(zip(before, after)) if a != b] if after else ["?"]
        print(f"{payload_hex:24} -> {answer} | changes: {', '.join(changes) or 'none'}")
        if after:
            before = after


if __name__ == "__main__":
    main()
