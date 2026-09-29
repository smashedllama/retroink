#pragma once
#include <IcsCalendar.h>

// Downloads the calendar feed, keeps the events around today, and stores them
// on the SD card for the Desk Calendar. The feed is read as it arrives and
// never held in memory whole.
namespace CalendarSync {

enum class Result : uint8_t {
  Ok,
  NotConfigured,
  NoDate,        // clockless reader and no date picked yet
  Network,       // could not reach the address
  NotCalendar,   // reached something that is not an iCalendar feed
  WriteFailed,
};

// Wi-Fi must already be connected. `eventCount` receives the number of stored
// entries, `truncated` whether limits dropped some.
Result run(size_t* eventCount = nullptr, bool* truncated = nullptr);

const char* resultText(Result result);

// The stored calendar, loaded once and kept until the next sync. False if
// nothing has been synced yet.
bool loadStored(ics::Calendar& out);
// The stored calendar's sync day, or 0.
bool hasStored();

}  // namespace CalendarSync
