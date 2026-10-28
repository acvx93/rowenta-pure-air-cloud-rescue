#include "board.h"

// Gizwits commands (module <-> board)
constexpr uint8_t CMD_INFO = 0x01, CMD_COMMAND = 0x03, CMD_REPLY = 0x04, CMD_REPORT = 0x05,
                  CMD_REPORT_ACK = 0x06, CMD_HEARTBEAT = 0x07, CMD_PAIRING = 0x09,
                  CMD_PAIRING_ACK = 0x0A, CMD_RESET_WIFI = 0x0B, CMD_RESET_WIFI_ACK = 0x0C,
                  CMD_WIFI_STATUS = 0x0D;
constexpr uint8_t ACTION_WRITE = 0x01, ACTION_READ = 0x02, ACTION_READ_STATUS = 0x03,
                  ACTION_REPORT = 0x04;

// Write (found by trial): 01 | field mask (2 bytes) | bits | 38 numeric values.
// The board only applies the fields whose mask bit is set, but always reads the whole frame:
// a short frame makes it use leftover memory as values.
constexpr uint16_t FIELD_POWER = 0x0004;  // value: bit 0 of "bits"
constexpr uint16_t FIELD_MODE = 0x0008;   // value: N[0]
constexpr uint16_t FIELD_LIGHT = 0x2000;  // value: N[6]
constexpr size_t NUMERIC_VALUES = 38;

constexpr uint32_t HEARTBEAT_MS = 30000, READ_MS = 30000, WIFI_MS = 10000;

void Board::begin(HardwareSerial &uart, int rxPin, int txPin) {
  uart_ = &uart;
  uart_->begin(9600, SERIAL_8N1, rxPin, txPin);
  send(CMD_INFO, nullptr, 0, ++sn_);  // the board stays silent until the "module" introduces itself
}

void Board::loop() {
  while (uart_->available()) receive(uart_->read());
  uint32_t now = millis();
  if (now - lastHeartbeat_ >= HEARTBEAT_MS) {
    lastHeartbeat_ = now;
    send(CMD_HEARTBEAT, nullptr, 0, ++sn_);
  }
  if (now - lastRead_ >= READ_MS) {
    lastRead_ = now;
    uint8_t read = ACTION_READ;
    send(CMD_COMMAND, &read, 1, ++sn_);
  }
  if (now - lastWifi_ >= WIFI_MS) {
    lastWifi_ = now;
    uint8_t data[2] = {uint8_t(wifiBits_ >> 8), uint8_t(wifiBits_)};
    send(CMD_WIFI_STATUS, data, 2, ++sn_);
  }
}

void Board::setPower(bool on) { write(FIELD_POWER, on ? 1 : 0, 0, 0); }

void Board::powerOnInMode(uint8_t mode) {
  if (mode < MODE_SILENT || mode > MODE_TURBO) return;
  write(FIELD_POWER | FIELD_MODE, 1, mode, 0);
}

void Board::setMode(uint8_t mode) {
  if (mode < MODE_SILENT || mode > MODE_TURBO) return;  // mode 0 leaves the motor stopped
  write(FIELD_MODE, 0, mode, 0);
}

void Board::setLight(uint8_t light) {
  if (light > LIGHT_OFF) return;
  write(FIELD_LIGHT, 0, 0, light);
}

// Gizwits WiFi status bits: 1 = station mode, 4 = connected to router, 5 = connected to cloud,
// 8-10 = signal level.
void Board::reportWifi(bool connected, int rssi) {
  if (!connected) {
    wifiBits_ = 0;
    return;
  }
  uint16_t level = constrain(map(rssi, -90, -40, 0, 7), 0, 7);
  wifiBits_ = (1 << 1) | (1 << 4) | (1 << 5) | (level << 8);
}

void Board::write(uint16_t fields, uint8_t bits, uint8_t mode, uint8_t light) {
  uint8_t data[4 + NUMERIC_VALUES];
  data[0] = ACTION_WRITE;
  data[1] = fields >> 8;
  data[2] = fields & 0xFF;
  data[3] = bits;
  memset(data + 4, 0x02, NUMERIC_VALUES);  // harmless filler for the unflagged fields
  data[4 + 0] = mode ? mode : MODE_AUTO;
  data[4 + 6] = light;
  send(CMD_COMMAND, data, sizeof(data), ++sn_);
  lastRead_ = millis() - READ_MS + 300;  // read the state back right away
}

void Board::send(uint8_t cmd, const uint8_t *data, size_t len, uint8_t sn) {
  uint16_t length = len + 5;  // cmd, sn, 2 flag bytes, data, checksum
  uint8_t body[4 + 4 + 50];
  size_t n = 0;
  body[n++] = length >> 8;
  body[n++] = length & 0xFF;
  body[n++] = cmd;
  body[n++] = sn;
  body[n++] = 0;
  body[n++] = 0;
  memcpy(body + n, data, len);
  n += len;
  uint8_t sum = 0;
  for (size_t i = 0; i < n; i++) sum += body[i];
  body[n++] = sum;

  uart_->write(0xFF);
  uart_->write(0xFF);
  for (size_t i = 0; i < n; i++) {
    uart_->write(body[i]);
    if (body[i] == 0xFF) uart_->write(0x55);  // escape 0xFF inside the frame
  }
}

void Board::receive(uint8_t b) {
  if (length_ >= 2 && escaped_ && b == 0x55) {  // FF 55 inside the frame = FF
    escaped_ = false;
    return;
  }
  escaped_ = length_ >= 2 && b == 0xFF;
  if (length_ < 2 && b != 0xFF) {  // outside a frame: noise (the board pulls the line low between frames)
    length_ = 0;
    return;
  }
  if (length_ >= sizeof(frame_)) {
    length_ = 0;
    return;
  }
  frame_[length_++] = b;
  if (length_ >= 4 && length_ == 4u + ((frame_[2] << 8) | frame_[3])) {
    process();
    length_ = 0;
    escaped_ = false;
  }
}

void Board::process() {
  uint8_t sum = 0;
  for (size_t i = 2; i < length_ - 1; i++) sum += frame_[i];
  if (sum != frame_[length_ - 1]) return;

  uint8_t cmd = frame_[4], sn = frame_[5];
  const uint8_t *data = frame_ + 8;
  size_t dataLen = length_ - 9;

  switch (cmd) {
    case CMD_REPORT:
      send(CMD_REPORT_ACK, nullptr, 0, sn);
      if (dataLen > 1 && data[0] == ACTION_REPORT) parseState(data + 1, dataLen - 1);
      break;
    case CMD_REPLY:
      if (dataLen > 1 && data[0] == ACTION_READ_STATUS) parseState(data + 1, dataLen - 1);
      break;
    case CMD_PAIRING:
      send(CMD_PAIRING_ACK, nullptr, 0, sn);
      if (onPairingRequest) onPairingRequest();
      break;
    case CMD_RESET_WIFI:
      send(CMD_RESET_WIFI_ACK, nullptr, 0, sn);
      break;
  }
}

void Board::parseState(const uint8_t *st, size_t len) {
  if (len < 34) return;
  PurifierState next;
  next.valid = true;
  next.lock = st[0] & 0x10;
  next.power = st[1] & 0x01;
  next.pm25 = (st[2] << 8) | st[3];
  next.mode = st[5];
  next.speed = st[6];
  next.light = st[31];
  bool changed = !state_.valid || next.power != state_.power || next.mode != state_.mode ||
                 next.speed != state_.speed || next.light != state_.light ||
                 next.lock != state_.lock || next.pm25 != state_.pm25;
  state_ = next;
  if (changed && onStateChange) onStateChange();
}
