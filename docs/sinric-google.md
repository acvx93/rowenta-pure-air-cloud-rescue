# Google Home, Gemini and remote air quality with Sinric Pro

[Sinric Pro](https://sinric.pro) is a service that connects DIY devices to Google Home and Alexa.
Its **free plan includes 3 devices**, which is exactly what this project uses:

| Sinric Pro device type | Used for | Needed? |
|---|---|---|
| **Fan** | Power and speed (= mode) of the purifier | Yes |
| **Air Quality Sensor** | PM2.5 reading in the Sinric Pro app, and alerts | Recommended |
| **Dimmable Switch** | The purifier's light | Optional (see limitations) |

No Google Nest speaker or hub is needed: the Google Home app and Gemini on your phone are enough.

## 1. Create the devices

1. Create a free account at [sinric.pro](https://sinric.pro).
2. **Devices → Add device**:
   - Type **Fan** (the one with speed levels), name it for example *Purifier*. Set the
     **maximum fan speed to 4**. The four speeds map to the purifier's modes:

     | Speed | Mode |
     |---|---|
     | 1 | Night |
     | 2 | Silent |
     | 3 | Auto |
     | 4 | Turbo |

   - Type **Air Quality Sensor**, name it for example *Air quality*.
   - Optional: type **Dimmable Switch**, name it for example *Purifier light*.
3. If a device has an option to restore its last state when it connects, turn it off: the
   purifier itself is the source of truth.

## 2. Enter the keys on the purifier

1. In Sinric Pro, open **Credentials** and copy the **App Key** and **App Secret**. Copy the
   **Device ID** of each device from the device list.
2. Open `http://purifier.local` and fill in the *Google Home (Sinric Pro)* card, in this order:
   **Device ID** (the Fan), **App Key**, **App Secret**, then the light and air sensor device IDs
   if you created them, and the alert threshold.
3. Press **Save and restart**. After a few seconds the card shows **Connected**.

The App Secret is stored only on the ESP32. The page never shows it again (it says *saved*), and
it is never transmitted: it is used only to sign the messages exchanged with Sinric Pro.

## 3. Link Google Home

1. In the **Google Home** app: **+ → Device → Works with Google**, search for **Sinric Pro** and
   sign in with your Sinric Pro account.
2. The purifier appears as a fan, and the sensor as an air quality device. Assign them to a room.

Example voice commands (Google Assistant / Gemini, in your phone's language):

- "Turn off the purifier" / "Turn on the purifier": tested and working.
- "Set the purifier speed to 4": should set Turbo, but this voice command has **not been tested**.
  Changing the speed from the Sinric Pro app was tested and works; in Google Home the speed names
  are odd, see below.

## 4. Air quality away from home, and alerts

- The **Sinric Pro app** (Android/iOS) shows the purifier and the PM2.5 reading from anywhere.
  The firmware sends it every 65 s (Sinric Pro rejects sensor updates less than 60 s apart).
- When the reading reaches the **alert threshold** set on the web page (default 35), you get a
  **push notification** in the Sinric Pro app, mentioning whether the purifier is off. A new
  alert is sent only after the level has gone back below 70 % of the threshold.
- If signing in to the Sinric Pro app **with Google fails**, use **Forgot password** with the
  same e-mail address to create a password, and sign in with it.

## Known limitations (tested)

- **Speed names in Google Home**: the speed list shows "low / medium / high / 1 / 2 / 3 / 4".
  Use the numbers 1–4. In the Sinric Pro app the 4 speeds work as expected.
- **Air quality in Google Home** refreshes rarely (it may show an old value for hours). The
  Sinric Pro app and the local web page are up to date.
- **Light from Google** works as on/off only; the dim level is available on the local page.
  If you don't need the light in Google, skip the Dimmable Switch and keep a free device slot.
- Push alerts come from the *Fan* device: the SinricPro library 3.5.2 can't send notifications
  from the air quality sensor (a bug in the library).
- Sinric Pro is a third-party cloud service; if it changes its free plan or shuts down, voice
  control stops, but the local web page and the schedule keep working.
