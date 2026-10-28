# Reference captures

Logic analyzer recordings of the serial link between the purifier's control board and the
**original** WiFi module, made with a 24 MHz 8-channel analyzer and sigrok. They are here so you
can compare them with your own purifier, or study the protocol without opening anything.

| File | What happens | Decoded |
|---|---|---|
| `idle.sr` | Purifier on standby, nothing pressed: status reports and WiFi status every few seconds | [idle.txt](idle.txt) |
| `buttons.sr` | Panel buttons pressed during ~150 s: power on (status byte 1 goes `00`→`01`), LOCK on and off (byte 0 bit `0x10`); byte 3 is the particle sensor moving on its own | [buttons.txt](buttons.txt) |
| `cn1.sr` | Another standby recording, made while identifying the pins of the module's 5-pin harness (CN1) | [cn1.txt](cn1.txt) |

A recording of the module's boot exchange is not published: it contains the board's
`device info` reply, which may hold identifiers of the manufacturer's cloud.

## Recording settings

- Sample rate 1 MHz, channels D0 and D1.
- **D0 (CH1)** = module TX (module → board), **D1 (CH2)** = module RX (board → module), GND to
  the module's GND.
- UART 9600 baud, 8N1, not inverted. Note that the board pulls its TX line **low** between frames;
  decoders may show framing errors there, which are harmless.

## Opening them

- **PulseView**: File → Open, then add the *UART* decoder on D0 and again on D1 with 9600 baud.
- **Command line**: `python tools/capture.py show idle` (decodes with sigrok-cli and the Gizwits
  helpers, and prints which status bytes change between reports).

The status bytes and commands are explained in [docs/protocol.md](../docs/protocol.md); how to
make your own recordings is in [docs/research-tools.md](../docs/research-tools.md).
