#include "schedule.h"

#include <Preferences.h>

static Preferences prefs;

// Storage names are legacy (Spanish) and kept so existing devices keep their schedule.
constexpr const char *NVS_NAMESPACE = "programa", *NVS_KEY = "eventos";

void Schedule::begin() {
  prefs.begin(NVS_NAMESPACE, false);
  JsonDocument doc;
  if (deserializeJson(doc, prefs.getString(NVS_KEY, "[]")) == DeserializationError::Ok) {
    fromJson(doc.as<JsonArrayConst>());  // also rewrites schedules saved with the legacy keys
  }
}

std::vector<Event> Schedule::due(const tm &now) {
  std::vector<Event> result;
  int minuteOfDay = now.tm_hour * 60 + now.tm_min;
  if (minuteOfDay == lastMinute_) return result;
  lastMinute_ = minuteOfDay;
  uint8_t dayBit = 1 << ((now.tm_wday + 6) % 7);  // tm_wday: 0 = Sunday
  for (const Event &e : events_) {
    if ((e.days & dayBit) && e.hour == now.tm_hour && e.minute == now.tm_min) result.push_back(e);
  }
  return result;
}

void Schedule::toJson(JsonArray list) const {
  for (const Event &e : events_) {
    JsonObject o = list.add<JsonObject>();
    o["days"] = e.days;
    o["hour"] = e.hour;
    o["minute"] = e.minute;
    o["action"] = e.action;
  }
}

bool Schedule::fromJson(JsonArrayConst list) {
  std::vector<Event> events;
  for (JsonObjectConst o : list) {
    // Fallback to the legacy keys used by the first firmware version.
    Event e{o["days"] | (o["dias"] | 0), o["hour"] | (o["hora"] | 0), o["minute"] | (o["minuto"] | 0),
            o["action"] | (o["accion"] | 0)};
    if (e.days == 0 || e.days > 0x7F || e.hour > 23 || e.minute > 59 || e.action > 4) return false;
    events.push_back(e);
  }
  if (events.size() > MAX_EVENTS) return false;
  events_ = events;
  save();
  return true;
}

void Schedule::save() const {
  JsonDocument doc;
  toJson(doc.to<JsonArray>());
  String text;
  serializeJson(doc, text);
  prefs.putString(NVS_KEY, text);
}
