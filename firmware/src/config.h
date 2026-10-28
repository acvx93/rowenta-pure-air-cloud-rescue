// Settings you may want to change before building.
#pragma once

// UART to the purifier's CN6 header, through the level shifter:
// GPIO17 (ESP TX) -> LV1/HV1 -> CN6 RXD, CN6 TXD -> HV2/LV2 -> GPIO18 (ESP RX).
constexpr int PIN_TX = 17, PIN_RX = 18;

// The web page is at http://<HOSTNAME>.local; OTA uploads use the same name.
constexpr const char *HOSTNAME = "purifier";

// WiFi network the ESP32 creates when it knows no network (or when the panel WiFi button is held).
constexpr const char *PORTAL_NAME = "Purifier-Setup";

// POSIX timezone for the schedule. Default: Spain (Madrid). Find yours at
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
constexpr const char *TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3";
