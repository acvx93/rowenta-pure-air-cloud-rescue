# Troubleshooting

Problems met while building this project, and what fixed them.

## The web page shows "No data"

- The ESP32 gets no status from the board. Check the wiring against
  [hardware.md](hardware.md#wiring): TXD → HV2/LV2 → GPIO18 and RXD ← HV1/LV1 ← GPIO17. Swapped
  data lines are the most common mistake.
- Check that the shifter's **HV is at 5 V and LV at 3.3 V** with a multimeter, and that all
  grounds are connected.
- The board only starts talking after the "module" speaks; the firmware does that at boot and
  every 30 s, so wait half a minute after power-up.

## The Dupont wires keep falling off CN6

The CN6 pins are short and the Dupont plugs are loose on them. If a reading flickers or stops,
press the plugs on again, then hold all five together with tape, or use a 5-pin JST XH cable.

## I can't reach `http://purifier.local`

- Some Android devices and older systems don't resolve `.local` names: use the IP address shown
  in your router's device list.
- While the `Purifier-Setup` network is active, the page is not served (the setup page uses the
  same port). Finish the WiFi setup or wait 5 minutes for the setup network to close.
- The ESP32 only joins **2.4 GHz** networks.

## The setup network doesn't appear / I changed my WiFi

Hold the **WiFi button** on the purifier's panel: the `Purifier-Setup` network opens for 5 minutes.

## The motor doesn't turn and there is a high-pitched whine

This happened during the protocol experiments when invalid commands set **mode 0**. The firmware
in this repo never sends such commands. If it happens anyway (for example after your own
experiments), see [the mode 0 incident](protocol.md#the-mode-0-incident) for the frame that
recovered the board.

## Sinric Pro shows the device offline

- Check the card on the web page: *Not configured* means a key is missing; *Configured, not
  connected* means the keys are wrong or there's no internet.
- Re-enter the App Key and App Secret (copy them again from Sinric Pro's *Credentials* page) and
  press **Save and restart**. The log at `http://purifier.local/api/log` shows
  `Sinric Pro connected` when it works.

## Google Home doesn't show the fan speeds

- Make sure the Sinric Pro device type is **Fan** with maximum speed 4 (not the fan type with
  power levels only).
- Then ask Google to resync ("Hey Google, sync my devices"), or unlink and relink Sinric Pro in
  the Google Home app.

## Google says it turned the purifier off, but nothing happened

Check `http://purifier.local/api/log`: every command that arrives from Sinric Pro is logged. If
it's there, the command reached the purifier; in **Night** mode the changes are hard to notice
because the fan is very quiet and the lights are off.

## The air quality in Google Home is stuck

Google Home refreshes it rarely. Check the value in the Sinric Pro app or on the local page; the
log shows `Air sensor: send accepted` when the updates are going through.

## The upload over WiFi fails

- The `include/ota_password.h` and `include/ota_password.ini` files must exist and contain the
  same password **when you flash over USB the first time**; see
  [firmware/README.md](../firmware/README.md).
- If `purifier.local` doesn't resolve, pass the IP: `pio run -e ota -t upload --upload-port 192.168.x.y`.
