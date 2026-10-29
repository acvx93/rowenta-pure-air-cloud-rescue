# Purifier firmware

Firmware for the ESP32-S3 that replaces the purifier's WiFi module. Built with
[PlatformIO](https://platformio.org).

| File | Contents |
|---|---|
| [src/config.h](src/config.h) | Pins, hostname, setup network name, **timezone** |
| [src/board.h](src/board.h) / [.cpp](src/board.cpp) | Gizwits protocol: talks to the control board |
| [src/schedule.h](src/schedule.h) / [.cpp](src/schedule.cpp) | Weekly schedule stored in flash |
| [src/page.h](src/page.h) | The web page |
| [src/main.cpp](src/main.cpp) | WiFi, web API, Sinric Pro, OTA |

## Before the first upload

1. Edit [src/config.h](src/config.h) if needed. The default timezone is Spain (Madrid); find
   yours in the [POSIX timezone list](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv).
2. **Set your OTA password** (needed to update over WiFi later):
   - copy `include/ota_password.example.h` to `include/ota_password.h`;
   - copy `include/ota_password.example.ini` to `include/ota_password.ini`;
   - replace `change-me` in **both** with the same long random password.

   Both files are ignored by git, so your password never ends up in a commit. Without them the
   firmware still builds, with OTA updates disabled (you'll see a warning).

## Upload

```sh
pio run -e usb -t upload     # first time, over USB
pio run -e ota -t upload     # later, over WiFi (http://purifier.local)
```

Serial log: `pio device monitor` (115200 baud) on the ESP32's native USB port.
