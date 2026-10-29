# Background

## The app stopped working

The Rowenta Intense Pure Air Connect purifiers (and their siblings) were sold as "connected":
a WiFi module inside the purifier talked to Rowenta's cloud, and the **Pure Air by Rowenta**
app on the phone showed the air quality and let you control and schedule the purifier.

In 2026 the app stopped working for these purifiers. Logging in failed with an error, and the
purifier could no longer be controlled or scheduled remotely. Looking closer, the login itself
still worked, but the account no longer listed any product: the service behind it was gone.
We could not find an official announcement from Rowenta about the end of the service; product
listings and reviews simply describe the app as discontinued. The purifier still works from its
own buttons, but everything "smart" about it was lost.

## Looking for a way back

The first idea was to keep the original WiFi module and talk to it locally. The module turned out
to be a **BroadLink 3303-SBSL** with firmware customised for the manufacturer (label
`Code: SEB 0-0-11`). It exposes no useful local service: it only knows how to talk to a cloud
that no longer answers. Reverse engineering that cloud was not an option, and not something this
project wants to publish anyway.

So the plan changed: **replace the module**. The module is a small board plugged into the
purifier's control board with a 5-wire cable. Whatever travels on that cable is the whole
interface between "the smart part" and the purifier. If a cheap microcontroller could speak the
same language, it could take the module's place.

## Listening to the cable

1. The module's back has labelled test pads: `GND`, `VCC-3.3`, `TX`, `RX`, `PIN9`. Thin wires
   were soldered to them, and a multimeter confirmed 3.3 V logic levels, with `PIN9` going high
   in pairing mode.
2. A cheap 24 MHz USB **logic analyzer** recorded both data lines while the purifier was idle,
   while panel buttons were pressed, and while the module started up. (Saleae's own software
   refuses these clones; the open-source **sigrok / PulseView** works.)
3. The recordings decoded as **9600 baud serial** with frames starting with `FF FF`: the
   **Gizwits** serial protocol, a common protocol for WiFi modules in Chinese-made appliances.
   With the protocol known, the board's periodic **status reports** could be decoded and each
   button press matched to a byte change: power, mode, particle sensor, motor speed, panel lock,
   light.

The tools and recordings are in this repository: see [research-tools.md](research-tools.md) and
[../captures/](../captures/).

## Talking to the board

Next, an ESP32-S3 replaced the module, wired to the board's CN6 header through a level shifter
(the board uses 5 V logic, the ESP32 3.3 V). Running as a simple USB-to-serial bridge, it let the
PC impersonate the module:

- The board stays silent until the "module" introduces itself or sends a heartbeat.
- A **read** command returns the full status on demand.
- The **write** format was not documented anywhere. It was found by trial: a command byte, a
  2-byte mask of which fields to change, a byte of on/off values and a block of numeric values.

That trial and error had one scary moment: some early commands were too short, and the board
filled the missing values with leftovers. It ended up in **mode 0**, a mode that doesn't exist.
The motor stopped, a high-pitched whine appeared, and it **survived unplugging** the purifier.
Replaying one of the earlier commands exactly, which set a valid mode, brought it back. The
lesson is documented in [protocol.md](protocol.md): always send full-length commands, never
mode 0.

## The result

The final firmware runs on the ESP32 permanently inside the purifier. It plays the role of the
original module (heartbeats, acknowledgements, WiFi status) and adds what the cloud used to
provide, and a bit more:

- a local web page and JSON API,
- a weekly schedule stored on the device,
- Google Home / Gemini voice control and remote air quality through Sinric Pro,
- alerts when the particle level is high,
- firmware updates over WiFi.

Everything needed to repeat it is in this repository.
