#pragma once

#include <cstddef>
#include <cstdint>

// The date the Moon Phase, Earth, and Desk Calendar accessories show.
//
// With a clock chip that is simply "now", in the device's own time zone; a
// date picked there is only looked at until the accessory closes. The
// original X4 has no clock, so there the user picks a date (and, for Earth,
// a time and time zone) and it is saved to the SD card: the accessories
// reopen on it, the picker starts from it, and the matching sleep screens
// draw it.
struct DeskDateTime {
  uint16_t year = 2026;
  uint8_t month = 1;   // 1-12
  uint8_t day = 1;     // 1-31
  uint8_t hour = 12;   // local time, 0-23
  uint8_t minute = 0;  // local time, 0-59
  uint8_t zoneQ = 48;  // UTC offset in quarter hours, biased by 48 (as SETTINGS.clockUtcOffsetQ)
};

namespace DeskDate {

constexpr uint16_t kMinYear = 1970;
constexpr uint16_t kMaxYear = 2100;
constexpr uint8_t kMaxZoneQ = 104;

// What the accessories should show: local "now" on hardware with a clock,
// otherwise the saved date. False only on clockless hardware before a date
// has ever been picked.
bool current(DeskDateTime& out);

// The saved date, or false if none has been picked yet.
bool loadSaved(DeskDateTime& out);
void save(const DeskDateTime& value);

// A starting point for the picker when current() has nothing: the saved
// zone (or the settings zone), noon on 1 January 2026.
DeskDateTime fallback();

int daysInMonth(int year, int month);
// Clamps every field into range, including the day against its month.
void normalize(DeskDateTime& value);

// The same instant in UTC, which is what the moon and sun math expect.
void toUtc(const DeskDateTime& value, uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& hour,
           uint8_t& minute);

// "Sep 25, 2026" in the user's date format, "14:30" (or "2:30 PM"), and
// "UTC-5".
void formatDate(const DeskDateTime& value, char* buf, size_t len);
void formatTime(const DeskDateTime& value, char* buf, size_t len);
void formatZone(uint8_t zoneQ, char* buf, size_t len);

}  // namespace DeskDate
