// Talks to the purifier's control board (Gizwits serial protocol, 9600 8N1).
// The ESP32 plays the role of the original WiFi module: it acknowledges the board's reports,
// sends heartbeats and gives it commands.
#pragma once
#include <Arduino.h>
#include <functional>

enum Mode : uint8_t { MODE_SILENT = 1, MODE_NIGHT = 2, MODE_AUTO = 3, MODE_TURBO = 4 };
enum Light : uint8_t { LIGHT_ON = 0, LIGHT_DIM = 1, LIGHT_OFF = 2 };

struct PurifierState {
  bool valid = false;
  bool power = false;
  uint8_t mode = 0;
  uint8_t speed = 0;  // actual motor speed (1-5)
  uint8_t light = 0;  // 0 on, 1 dim (level 1), 2 off (night mode turns it off)
  bool lock = false;
  uint16_t pm25 = 0;  // particle sensor
};

class Board {
 public:
  void begin(HardwareSerial &uart, int rxPin, int txPin);
  void loop();
  const PurifierState &state() const { return state_; }

  void setPower(bool on);
  void powerOnInMode(uint8_t mode);
  void setMode(uint8_t mode);
  void setLight(uint8_t light);
  void reportWifi(bool connected, int rssi);

  std::function<void()> onStateChange;     // after every report that changes something
  std::function<void()> onPairingRequest;  // panel WiFi button held

 private:
  void send(uint8_t cmd, const uint8_t *data, size_t len, uint8_t sn);
  void write(uint16_t fields, uint8_t bits, uint8_t mode, uint8_t light);
  void receive(uint8_t b);
  void process();
  void parseState(const uint8_t *st, size_t len);

  HardwareSerial *uart_ = nullptr;
  PurifierState state_;
  uint8_t sn_ = 0;
  uint8_t frame_[300];
  size_t length_ = 0;
  bool escaped_ = false;
  uint32_t lastHeartbeat_ = 0, lastRead_ = 0, lastWifi_ = 0;
  uint16_t wifiBits_ = 0;
};
