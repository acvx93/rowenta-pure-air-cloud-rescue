"""Gizwits serial protocol helpers: build, split and describe frames.

Frame: FF FF | length (2 bytes, big endian) | cmd | sn | flags (2 bytes) | payload | checksum
The length counts from cmd to checksum; the checksum is the sum of the bytes from length to
the end of the payload. Any FF after the header is sent as FF 55.
"""

COMMANDS = {
    0x01: "get device info", 0x02: "device info",
    0x03: "command/read", 0x04: "read reply",
    0x05: "status report", 0x06: "status report ack",
    0x07: "heartbeat", 0x08: "heartbeat ack",
    0x09: "enter pairing", 0x0A: "pairing ack",
    0x0B: "reset WiFi", 0x0C: "reset WiFi ack",
    0x0D: "WiFi status", 0x0E: "WiFi status ack",
    0x0F: "reboot board", 0x10: "reboot ack",
    0x11: "illegal message", 0x12: "illegal message ack",
    0x17: "get network time", 0x18: "network time",
}
ACTIONS = {0x01: "write", 0x02: "read", 0x03: "read status", 0x04: "status"}


def build_frame(cmd, sn, payload=b"", flags=b"\x00\x00"):
    body = bytes([cmd, sn]) + flags + payload
    body = len(body + b"\x00").to_bytes(2, "big") + body  # the length includes the checksum
    raw = b"\xFF\xFF" + body + bytes([sum(body) & 0xFF])
    return raw[:2] + raw[2:].replace(b"\xFF", b"\xFF\x55")


def frames(byte_stream):
    """Group (time, byte) pairs into FF FF <len16> <body> frames, undoing the FF 55 escape."""
    buf, start, escaped = [], 0.0, False
    for t, b in byte_stream:
        if escaped and b == 0x55:
            escaped = False
            continue  # FF 55 inside the body = literal FF
        escaped = len(buf) >= 2 and b == 0xFF
        if len(buf) < 2 and b != 0xFF:
            buf = []
            continue
        if not buf:
            start = t
        buf.append(b)
        if len(buf) >= 4 and len(buf) == 4 + (buf[2] << 8 | buf[3]):
            yield start, bytes(buf)
            buf, escaped = [], False


def describe(frame):
    """Return (ASCII description, cmd, payload) for one unescaped frame."""
    body, checksum = frame[2:-1], frame[-1]
    cmd, sn, payload = frame[4], frame[5], frame[8:-1]
    ok = "" if sum(body) & 0xFF == checksum else "  BAD CHECKSUM!"
    text = f"{COMMANDS.get(cmd, f'cmd 0x{cmd:02X}')} sn={sn:02X}"
    if cmd in (0x03, 0x04, 0x05) and payload:
        text += f" [{ACTIONS.get(payload[0], f'action 0x{payload[0]:02X}')}]"
    return text + ok, cmd, payload
