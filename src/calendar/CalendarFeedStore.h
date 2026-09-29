#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <string>

struct CalendarFeedConfig {
  bool enabled = false;
  // The calendar's secret iCal address (Google "secret address in iCal format",
  // iCloud public calendar link, Outlook published ICS link). webcal:// is
  // accepted and treated as https://.
  std::string url;
};

/**
 * The calendar feed link, kept as JSON on the SD card. The address works like
 * a password for the calendar, so it is obfuscated with the device key the
 * same way the Obsidian API key is.
 */
class CalendarFeedStore : public PersistableStore<CalendarFeedStore> {
  CalendarFeedConfig config;
  bool loaded_ = false;

  CalendarFeedStore() = default;
  friend class PersistableStore<CalendarFeedStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/calendar.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  void ensureLoaded() const;
  const CalendarFeedConfig& getConfig() const {
    ensureLoaded();
    return config;
  }
  bool setConfig(const CalendarFeedConfig& newConfig);
};

#define CALENDAR_FEED CalendarFeedStore::getInstance()
