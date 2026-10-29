from gizwits import build_frame, describe, frames

# Frame that rescued the board from the "mode 0" state (see docs/protocol.md).
RESCUE = bytes.fromhex("FFFF000A0303000001FF55FF55000110")


def test_build_frame_escapes_ff():
    assert build_frame(0x03, 0x03, bytes.fromhex("01FFFF0001")) == RESCUE


def test_frames_unescapes_and_splits():
    stream = [(i * 0.001, b) for i, b in enumerate(b"\x00\x12" + RESCUE + build_frame(0x07, 0x05))]
    got = [f for _, f in frames(stream)]
    assert got[0] == bytes.fromhex("FFFF000A0303000001FFFF000110")
    assert got[1][4] == 0x07 and len(got) == 2


def test_describe_names_command_and_action():
    frame = next(f for _, f in frames((0, b) for b in build_frame(0x03, 0x01, b"\x02")))
    text, cmd, payload = describe(frame)
    assert cmd == 0x03 and payload == b"\x02" and "read" in text and "CHECKSUM" not in text


def test_describe_flags_bad_checksum():
    bad = bytearray(build_frame(0x07, 0x01))
    bad[-1] ^= 1
    assert "BAD CHECKSUM" in describe(bytes(bad))[0]


def test_describe_is_ascii():
    for cmd in range(0x01, 0x19):
        describe(build_frame(cmd, 1, b"\x01"))[0].encode("ascii")


def test_samplerate_is_read_from_sigrok_show_output():
    from capture import parse_samplerate
    assert parse_samplerate("Samplerate: 2000000\nChannels: 8\n") == 2_000_000
    assert parse_samplerate("Channels: 8\n") == 1_000_000  # default when sigrok doesn't say
