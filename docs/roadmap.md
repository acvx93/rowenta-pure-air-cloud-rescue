# Roadmap

Ideas for the future. None of them is promised; contributions are welcome.

- **Automatic mode by particles**: turn the purifier on (or to Turbo) when PM2.5 goes above a
  threshold, and back when it drops.
- **History and charts**: keep the particle readings for the last days on the ESP32 and draw
  them on the web page.
- **Compare with outdoor air**: fetch the outdoor air quality from a public API and show both.
- **Filter life**: identify the counters in the status bytes (they look like filter hours) and
  show the remaining filter life and a reminder.
- **More fields**: map the remaining write mask bits (timer, lock, ionizer) safely.
- **Matter / Home Assistant**: expose the purifier locally to Home Assistant (MQTT discovery) or
  as a Matter device, without any cloud.
- **More models**: confirm the models in the compatibility table and document differences.
- **Timezone from the web page** instead of a build setting.
