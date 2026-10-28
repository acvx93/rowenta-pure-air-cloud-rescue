// Weekly schedule: power on/off by day of the week, stored in flash.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

struct Event {
  uint8_t days;  // bit 0 = Monday ... bit 6 = Sunday
  uint8_t hour;
  uint8_t minute;
  uint8_t action;  // 0 = power off; 1-4 = power on in that mode
};

class Schedule {
 public:
  void begin();
  // Returns the events due in this minute (only once per minute).
  std::vector<Event> due(const tm &now);
  void toJson(JsonArray list) const;
  bool fromJson(JsonArrayConst list);  // validates, saves and replaces

  static constexpr size_t MAX_EVENTS = 16;

 private:
  void save() const;
  std::vector<Event> events_;
  int lastMinute_ = -1;
};
