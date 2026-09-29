#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// A small, streaming iCalendar (.ics) reader for the Desk Calendar.
//
// It reads a feed a few hundred bytes at a time, keeps only the events that
// land inside a window of dates around today, expands repeating events
// (daily, weekly, monthly, yearly) into individual days, and produces a compact
// list that fits comfortably in memory and on the SD card. Plain C++ with no
// hardware dependencies, so it is unit-tested on a computer.
//
// Times are turned into the reader's local time: a time ending in Z (UTC) is
// shifted by the reader's UTC offset setting; anything else (floating times,
// or times naming a time zone) is taken as already being local. That is right
// for a calendar kept in one time zone and off by the difference for an event
// set in another.
namespace ics {

// Days since 1970-01-01 (the Unix epoch) for a civil date, and back.
int32_t daysFromCivil(int year, int month, int day);
void civilFromDays(int32_t days, int& year, int& month, int& day);
// 0 = Monday ... 6 = Sunday.
int weekdayFromDays(int32_t days);
int daysInMonth(int year, int month);

constexpr uint16_t kNoTime = 0xFFFF;  // "all day" as a start time; "unknown" as an end time

struct Occurrence {
  int32_t day = 0;                 // local date, days since 1970-01-01
  uint16_t startMinute = kNoTime;  // minutes after local midnight, kNoTime = all day
  uint16_t endMinute = kNoTime;    // end on the same day, kNoTime = not known
  uint16_t titleOffset = 0;        // byte offset of the title in Calendar::titles

  bool allDay() const { return startMinute == kNoTime; }
};

struct Calendar {
  int32_t syncedDay = 0;       // the local date this list was built on
  int32_t windowStartDay = 0;  // first date the list covers
  int32_t windowEndDay = 0;    // last date the list covers
  bool truncated = false;      // a limit was hit, so some events are missing
  std::vector<Occurrence> occurrences;  // sorted by day, all-day first, then by start time
  std::string titles;                   // NUL-terminated titles, each stored once

  const char* title(const Occurrence& occurrence) const;
  // Bit (day - 1) is set for every day of the month that has at least one event.
  uint32_t monthMask(int year, int month) const;
  // Indices into occurrences of the events on a day, in order.
  void eventsOn(int32_t day, std::vector<size_t>& out) const;
  // Up to `limit` indices of events on or after `fromDay`, in order.
  void upcoming(int32_t fromDay, size_t limit, std::vector<size_t>& out) const;
};

struct ParseOptions {
  int32_t windowStartDay = 0;
  int32_t windowEndDay = 0;
  int utcOffsetMinutes = 0;      // added to times ending in Z
  size_t maxOccurrences = 400;   // keeps memory bounded on the reader
  size_t maxTitleBytes = 12000;  // total size of all stored titles
};

class Parser {
 public:
  explicit Parser(const ParseOptions& options);

  // Feed the feed's bytes in order, in pieces of any size.
  void feed(const uint8_t* data, size_t length);
  // Call once after the last piece. The parser is spent afterwards.
  Calendar finish();

  // True once a BEGIN:VCALENDAR has been seen, so callers can tell a real
  // calendar from an error page or a login screen.
  bool sawCalendar() const { return sawCalendar_; }
  size_t eventsSeen() const { return eventsSeen_; }

 private:
  struct Event;
  struct Work;
  struct Rule;

  ParseOptions options_;
  bool sawCalendar_ = false;
  size_t eventsSeen_ = 0;
  bool truncated_ = false;

  std::string physical_;  // the physical line being received
  std::string logical_;   // the unfolded line waiting for its continuations
  bool inEvent_ = false;
  int alarmDepth_ = 0;

  std::vector<Work> work_;
  std::vector<std::pair<uint32_t, int32_t>> overrides_;  // (uid, original day) of moved or cancelled instances
  std::string titles_;
  std::vector<uint16_t> titleOffsets_;

  // The event being read; a pointer so this header stays small.
  Event* current_ = nullptr;

  void endPhysicalLine();
  void processLine(const std::string& line);
  void finishEvent();
  void expand(const Event& event, const Rule* rule);
  void emit(const Event& event, int32_t day, uint16_t titleOffset);
  bool titleFor(const Event& event, uint16_t& offset);

 public:
  ~Parser();
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
};

// A stable binary form for the SD card.
std::string serialize(const Calendar& calendar);
bool deserialize(const uint8_t* data, size_t length, Calendar& out);

}  // namespace ics
