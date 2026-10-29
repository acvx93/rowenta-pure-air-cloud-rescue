# Serial protocol

The WiFi module and the control board talk over CN6 with the **Gizwits** serial protocol (as used
by many WiFi modules in appliances). Everything below was observed on a PU4080F0; it was not
taken from any manufacturer documentation.

> [!CAUTION]
> **Writing to the board is risky.** Commands that are too short, or that set mode 0, left the
> author's board with the **motor stopped and a ~16 kHz whine**, a state that **survived
> unplugging** the purifier. Read [the incident](#the-mode-0-incident) before experimenting.

## Link

- UART, **9600 baud, 8N1**, not inverted, **5 V** levels on CN6.
- The board pulls its TXD line **low between frames** and raises it shortly before each frame, so
  analyzers may report framing errors between frames; they are harmless.

## Frame format

```
FF FF | length (2 bytes, big endian) | cmd | sn | flags (2 bytes, 00 00) | payload | checksum
```

- `length` counts the bytes from `cmd` to `checksum`.
- `checksum` = sum of the bytes from `length` to the end of the payload, modulo 256.
- `sn` is a sequence number; acknowledgements repeat the `sn` of the frame they answer.
- After the `FF FF` header, any `FF` byte is sent as **`FF 55`** (escape).

Example, heartbeat from the module: `FF FF 00 05 07 01 00 00 0D`.

## Commands

| cmd | Direction | Meaning |
|---|---|---|
| `01` / `02` | module → board / reply | Device info. The board answers with a ~130-byte binary block (not plain Gizwits product info). |
| `03` / `04` | module → board / reply | Read or write the status (see below) |
| `05` / `06` | board → module / ack | **Status report**, sent on every change and periodically. The module must acknowledge it. |
| `07` / `08` | module → board / reply | Heartbeat |
| `09` / `0A` | board → module / ack | WiFi button held: enter pairing mode |
| `0B` / `0C` | board → module / ack | Reset WiFi |
| `0D` / `0E` | module → board / ack | WiFi status (2 bytes of flags) |

Behaviour worth knowing:

- **The board stays silent until the "module" speaks**: send `01` (device info) or `07`
  (heartbeat) first.
- The original module sends heartbeats periodically and its WiFi status every 10 s (the
  firmware uses 30 s for heartbeats and status reads). WiFi
  status bits used by the firmware: bit 1 station mode, bit 4 connected to router, bit 5 connected
  to cloud, bits 8–10 signal level (0–7).

## Reading the status

Send `cmd 03` with payload `02` (read). The board answers `cmd 04` with payload `03` followed by
34 status bytes. The `05` status reports carry the same 34 bytes after the action byte `04`.

| Byte | Meaning |
|---|---|
| 0 | bit `0x10` = panel buttons locked (LOCK) |
| 1 | power: `00` off, `01` on |
| 2–3 | **PM2.5** particle reading (16-bit, big endian) |
| 5 | **mode**: `1` Silent, `2` Night, `3` Auto, `4` Turbo |
| 6 | actual motor speed, 1–5 |
| 31 | **light**: `0` on, `1` dim (level 1), `2` off |
| others | mostly constant counters (filter hours?), not identified |

Notes:

- Night mode turns the light off and ignores the LIGHT button; Auto also overrides the light.
- The TIMER and FILTER buttons don't change anything in the status.

## Writing

`cmd 03` with this payload (always the full length, **42 bytes**):

```
01 | F (2 bytes, big endian: field mask) | W (1 byte: on/off values) | N (38 bytes: numeric values)
```

The board only applies the fields whose bit is set in `F`, but it **always reads the whole
payload**: if the frame is shorter, it fills the missing values with leftovers from memory.

| Field | Mask bit in `F` | Value |
|---|---|---|
| Power | `0x0004` | bit 0 of `W` |
| Mode | `0x0008` | `N[0]`, 1–4 (**never 0**) |
| Light | `0x2000` | `N[6]`, 0–2 |

Other mask bits (speed, lock, timer…) were not mapped: don't set them. The firmware fills the
unused numeric values with `02` and always puts a valid mode in `N[0]`.

Examples, exactly as the firmware sends them (payloads, before framing; `N[0]` always holds a
valid mode and `N[6]` the light, even when their mask bits are not set):

- Power off: `01 00 04 00 | 03 02 02 02 02 02 00` + 31 × `02`
- Power on in Auto: `01 00 0C 01 | 03 02 02 02 02 02 00` + 31 × `02`
- Light off: `01 20 00 00 | 03 02 02 02 02 02 02` + 31 × `02`

The board answers with `cmd 04` and then sends a `05` status report with the new state.

## The mode 0 incident

During the first write experiments, short payloads and a misread mask set the mode to `0`. The
result:

- the motor stopped, and a quiet but constant **~16 kHz whine** appeared;
- the MODES button only changed the LEDs; power and lock still responded;
- **unplugging the purifier didn't help**: the state was stored.

It was fixed by replaying, byte for byte, an earlier command that had set a valid mode:

```
FF FF 00 0A 03 03 00 00 01 FF 55 FF 55 00 01 10
```

(cmd `03`, sn `03`, payload `01 FF FF 00 01`: all mask bits, mode 1). After it, the board reported
mode 1, speed 1, power off; powering on from the panel brought the motor back. This frame is
short and sets every field, so treat it as a **last resort**, not as an example of a good command.

Rules since then, followed by the firmware:

1. Always send the full 42-byte write payload.
2. Never send mode 0; validate every value before sending.
3. Only set mask bits whose meaning is known.
