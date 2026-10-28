// Rowenta PU4080 air purifier with an ESP32-S3 in place of the original WiFi module.
//
// - Talks to the control board through CN6 (via a level shifter), pins in config.h.
// - WiFi through WiFiManager: if it knows no network it creates the PORTAL_NAME network; also
//   when the panel WiFi button is held.
// - Web page at http://<HOSTNAME>.local with control and weekly schedule.
// - Google Home / Gemini through Sinric Pro (keys entered on the web page).
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SinricPro.h>
#include <SinricProAirQualitySensor.h>
#include <SinricProDimSwitch.h>
#include <SinricProFanUS.h>
#include <deque>
#include <WebServer.h>
#include <WiFiManager.h>

#if __has_include("ota_password.h")
#include <ArduinoOTA.h>
#include "ota_password.h"
#define OTA_ENABLED 1
#else
#warning "include/ota_password.h not found: OTA updates disabled (see firmware/README.md)"
#define OTA_ENABLED 0
#endif

#include "board.h"
#include "config.h"
#include "page.h"
#include "schedule.h"

Board board;
Schedule schedule;
WebServer web(80);
WiFiManager wifiManager;
Preferences settings;

String sinricKey, sinricSecret, sinricDevice, sinricLightDevice, sinricAirDevice;
bool sinricStarted = false;
PurifierState reported;  // last state sent to Sinric Pro

// Particle alert: fires above the threshold and not again until it drops below 70 % of it.
constexpr uint32_t AIR_SEND_MS = 65000;  // Sinric Pro rejects sensor values less than 60 s apart
uint16_t alertThreshold = 35;
bool alertActive = false;

// --- Log of what arrives from outside (shown at /api/log) ---------------------------------------

std::deque<String> logLines;

void logLine(const char *format, ...) {
  char text[120];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  tm t;
  char clock[10] = "--:--:--";
  if (getLocalTime(&t, 0)) strftime(clock, sizeof(clock), "%H:%M:%S", &t);
  logLines.push_back(String(clock) + " " + text);
  if (logLines.size() > 40) logLines.pop_front();
  Serial.println(logLines.back());
}

// --- Sinric Pro: the purifier is a "fan" with power and speed 1-4 -------------------------------
// (maximum fan speed set to 4 in Sinric Pro: 1 night, 2 silent, 3 auto, 4 turbo)

const uint8_t MODE_FOR_LEVEL[] = {MODE_NIGHT, MODE_SILENT, MODE_AUTO, MODE_TURBO};

int levelForMode(uint8_t mode) {
  for (int i = 0; i < 4; i++) {
    if (MODE_FOR_LEVEL[i] == mode) return i + 1;
  }
  return 3;
}

uint8_t modeForLevel(int level) {
  if (level > 4) level = (level + 24) / 25;  // in case it arrives as a percentage
  return MODE_FOR_LEVEL[constrain(level, 1, 4) - 1];
}

bool setSpeed(int &level) {
  if (level <= 0) {
    board.setPower(false);
    return true;
  }
  uint8_t mode = modeForLevel(level);
  board.powerOnInMode(mode);
  level = levelForMode(mode);
  return true;
}

// Light as a "dimmable switch": off = off, level <= 50 % = dim, higher = on.
int levelForLight(uint8_t light) { return light == LIGHT_ON ? 100 : light == LIGHT_DIM ? 50 : 0; }

uint8_t lightForLevel(int level) {
  if (level <= 0) return LIGHT_OFF;
  return level <= 50 ? LIGHT_DIM : LIGHT_ON;
}

void startSinric() {
  if (sinricStarted || sinricKey.isEmpty() || sinricSecret.isEmpty() || sinricDevice.isEmpty()) return;
  SinricProFanUS &fan = SinricPro[sinricDevice];
  fan.onPowerState([](const String &, bool &on) {
    logLine("Sinric fan: power=%d", on);
    board.setPower(on);
    return true;
  });
  fan.onRangeValue([](const String &, int &level) {
    logLine("Sinric fan: speed=%d", level);
    return setSpeed(level);
  });
  fan.onAdjustRangeValue([](const String &, int &delta) {
    if (abs(delta) > 4) delta = delta / 25;  // in case it arrives as a percentage
    int level = constrain(levelForMode(board.state().mode) + delta, 1, 4);
    logLine("Sinric fan: adjust speed %+d -> %d", delta, level);
    delta = level;  // the library expects the resulting level
    return setSpeed(delta);
  });

  if (!sinricLightDevice.isEmpty()) {
    SinricProDimSwitch &light = SinricPro[sinricLightDevice];
    light.onPowerState([](const String &, bool &on) {
      logLine("Sinric light: on=%d", on);
      board.setLight(on ? LIGHT_ON : LIGHT_OFF);
      return true;
    });
    light.onPowerLevel([](const String &, int &level) {
      logLine("Sinric light: level=%d", level);
      board.setLight(lightForLevel(level));
      level = levelForLight(lightForLevel(level));
      return true;
    });
    light.onAdjustPowerLevel([](const String &, int &delta) {
      int level = constrain(levelForLight(board.state().light) + delta, 0, 100);
      logLine("Sinric light: adjust %+d -> %d", delta, level);
      board.setLight(lightForLevel(level));
      delta = levelForLight(lightForLevel(level));
      return true;
    });
  }

  if (!sinricAirDevice.isEmpty()) {
    SinricProAirQualitySensor &air = SinricPro[sinricAirDevice];  // registers it; it only sends data
    (void)air;
  }

  SinricPro.onConnected([] { logLine("Sinric Pro connected"); });
  SinricPro.onDisconnected([] { logLine("Sinric Pro disconnected"); });
  SinricPro.begin(sinricKey, sinricSecret);
  sinricStarted = true;
}

void reportToSinric() {
  const PurifierState &s = board.state();
  if (!sinricStarted || !SinricPro.isConnected() || !s.valid) return;
  SinricProFanUS &fan = SinricPro[sinricDevice];
  if (!reported.valid || s.power != reported.power) fan.sendPowerStateEvent(s.power);
  if (s.power && (!reported.valid || s.mode != reported.mode)) fan.sendRangeValueEvent(levelForMode(s.mode));
  if (!sinricLightDevice.isEmpty() && (!reported.valid || s.light != reported.light)) {
    SinricProDimSwitch &light = SinricPro[sinricLightDevice];
    light.sendPowerStateEvent(s.light != LIGHT_OFF);
    light.sendPowerLevelEvent(levelForLight(s.light));
  }
  reported = s;
}

void sendAirQuality() {
  static uint32_t lastSend = 0;
  const PurifierState &s = board.state();
  if (sinricAirDevice.isEmpty() || !sinricStarted || !SinricPro.isConnected() || !s.valid) return;
  if (millis() - lastSend < AIR_SEND_MS) return;
  lastSend = millis();
  SinricProAirQualitySensor &air = SinricPro[sinricAirDevice];
  static int lastResult = -1;
  int result = air.sendAirQualityEvent(0, s.pm25, 0);
  if (result != lastResult) {
    logLine("Air sensor: send %s (%u)", result ? "accepted" : "rejected", s.pm25);
    lastResult = result;
  }

  // Alerts are sent from the fan device: in SinricPro 3.5.2 the air quality sensor cannot send
  // them (it befriends PushNotification<AirQualitySensor> instead of its own class).
  SinricProFanUS &fan = SinricPro[sinricDevice];
  char alert[100];
  if (!alertActive && s.pm25 >= alertThreshold) {
    snprintf(alert, sizeof(alert), "High particle level at home: %u (threshold %u)%s", s.pm25, alertThreshold,
             s.power ? "" : ". The purifier is off");
    if (fan.sendPushNotification(alert)) {
      alertActive = true;
      logLine("Alert sent: %s", alert);
    }
  } else if (alertActive && s.pm25 < alertThreshold * 7 / 10) {
    snprintf(alert, sizeof(alert), "Particle level back to normal: %u", s.pm25);
    if (fan.sendPushNotification(alert)) {
      alertActive = false;
      logLine("Notice sent: %s", alert);
    }
  }
}

// --- Web page -----------------------------------------------------------------------------------

String localTime() {
  tm t;
  if (!getLocalTime(&t, 0) || t.tm_year < 120) return "not synced";
  char text[6];
  strftime(text, sizeof(text), "%H:%M", &t);
  return text;
}

void replyJson(const JsonDocument &doc, int code = 200) {
  String text;
  serializeJson(doc, text);
  web.send(code, "application/json", text);
}

bool readBody(JsonDocument &doc) {
  if (deserializeJson(doc, web.arg("plain")) != DeserializationError::Ok) {
    web.send(400, "text/plain", "Invalid JSON");
    return false;
  }
  return true;
}

void setupWeb() {
  web.on("/", HTTP_GET, [] { web.send_P(200, "text/html", PAGE); });

  web.on("/api/status", HTTP_GET, [] {
    const PurifierState &s = board.state();
    JsonDocument doc;
    doc["valid"] = s.valid;
    doc["power"] = s.power;
    doc["mode"] = s.mode;
    doc["speed"] = s.speed;
    doc["light"] = s.light;
    doc["lock"] = s.lock;
    doc["pm25"] = s.pm25;
    doc["time"] = localTime();
    doc["rssi"] = WiFi.RSSI();
    doc["sinric"]["configured"] = !sinricDevice.isEmpty();
    doc["sinric"]["connected"] = sinricStarted && SinricPro.isConnected();
    replyJson(doc);
  });

  web.on("/api/command", HTTP_POST, [] {
    JsonDocument doc;
    if (!readBody(doc)) return;
    bool hasMode = doc["mode"].is<int>(), hasLight = doc["light"].is<int>();
    int mode = doc["mode"] | 0, light = doc["light"] | 0;
    if ((hasMode && (mode < MODE_SILENT || mode > MODE_TURBO)) || (hasLight && (light < LIGHT_ON || light > LIGHT_OFF))) {
      web.send(400, "text/plain", "Invalid mode or light");  // checked before anything reaches the board
      return;
    }
    if (hasMode) {
      board.powerOnInMode(mode);
    } else if (doc["power"].is<bool>()) {
      board.setPower(doc["power"]);
    }
    if (hasLight) board.setLight(light);
    web.send(204);
  });

  web.on("/api/schedule", HTTP_GET, [] {
    JsonDocument doc;
    schedule.toJson(doc.to<JsonArray>());
    replyJson(doc);
  });

  web.on("/api/schedule", HTTP_POST, [] {
    JsonDocument doc;
    if (!readBody(doc)) return;
    if (!schedule.fromJson(doc.as<JsonArrayConst>())) {
      web.send(400, "text/plain", "Invalid schedule");
      return;
    }
    web.send(204);
  });

  web.on("/api/log", HTTP_GET, [] {
    String text;
    for (const String &line : logLines) text += line + "\n";
    web.send(200, "text/plain; charset=utf-8", text);
  });

  web.on("/api/sinric", HTTP_GET, [] {
    JsonDocument doc;
    doc["appKey"] = sinricKey;
    doc["deviceId"] = sinricDevice;
    doc["lightDeviceId"] = sinricLightDevice;
    doc["airDeviceId"] = sinricAirDevice;
    doc["alertThreshold"] = alertThreshold;
    doc["secretSaved"] = !sinricSecret.isEmpty();  // the secret never leaves the ESP32
    replyJson(doc);
  });

  web.on("/api/sinric", HTTP_POST, [] {
    JsonDocument doc;
    if (!readBody(doc)) return;
    // Legacy storage names, see setup().
    settings.putString("key", doc["appKey"] | "");
    settings.putString("device", doc["deviceId"] | "");
    settings.putString("deviceLuz", doc["lightDeviceId"] | "");
    settings.putString("deviceAire", doc["airDeviceId"] | "");
    settings.putUShort("umbral", constrain(doc["alertThreshold"] | 35, 5, 999));
    String secret = doc["appSecret"] | "";
    if (!secret.isEmpty()) settings.putString("secret", secret);
    web.send(204);
    delay(500);
    ESP.restart();
  });
}

// The WiFiManager portal runs its own server on port 80: the page only works without it.
void serveWeb(bool connected) {
  static bool active = false;
  bool portal = wifiManager.getConfigPortalActive();
  if (!active && connected && !portal) {
    web.begin();
    active = true;
  } else if (active && portal) {
    web.stop();
    active = false;
  }
  if (active) web.handleClient();
}

// --- Startup and loop ---------------------------------------------------------------------------

void runSchedule() {
  tm now;
  if (!getLocalTime(&now, 0) || now.tm_year < 120) return;  // no internet time yet
  for (const Event &e : schedule.due(now)) {
    Serial.printf("Schedule %02d:%02d -> action %d\n", e.hour, e.minute, e.action);
    if (e.action == 0) {
      board.setPower(false);
    } else {
      board.powerOnInMode(e.action);
    }
  }
}

void setup() {
  Serial.begin(115200);
  board.begin(Serial1, PIN_RX, PIN_TX);
  board.onStateChange = reportToSinric;
  board.onPairingRequest = [] {
    Serial.println("WiFi button: opening the setup portal");
    wifiManager.startConfigPortal(PORTAL_NAME);
  };
  schedule.begin();

  // Storage names are legacy (Spanish) and kept so existing devices keep their settings.
  settings.begin("sinric", false);
  sinricKey = settings.getString("key", "");
  sinricSecret = settings.getString("secret", "");
  sinricDevice = settings.getString("device", "");
  sinricLightDevice = settings.getString("deviceLuz", "");
  sinricAirDevice = settings.getString("deviceAire", "");
  alertThreshold = settings.getUShort("umbral", 35);

  WiFi.setHostname(HOSTNAME);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(300);
  wifiManager.autoConnect(PORTAL_NAME);

  configTzTime(TIMEZONE, "pool.ntp.org", "time.google.com");
  MDNS.begin(HOSTNAME);
  setupWeb();
#if OTA_ENABLED
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
#endif
}

void loop() {
  board.loop();
  wifiManager.process();
  bool connected = WiFi.status() == WL_CONNECTED;
  board.reportWifi(connected, WiFi.RSSI());
  if (connected) {
    startSinric();
    if (sinricStarted) {
      SinricPro.handle();
      sendAirQuality();
    }
  }
  serveWeb(connected);
#if OTA_ENABLED
  ArduinoOTA.handle();
#endif
  runSchedule();
}
