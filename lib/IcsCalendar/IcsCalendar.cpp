#include "IcsCalendar.h"

#include <algorithm>
#include <climits>
#include <cstring>

namespace ics {

// ---------------------------------------------------------------- date math

int32_t daysFromCivil(int y, const int m, const int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void civilFromDays(int32_t z, int& year, int& month, int& day) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const int doe = z - era * 146097;
  const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp + (mp < 10 ? 3 : -9);
  year = yoe + era * 400 + (month <= 2);
}

int weekdayFromDays(const int32_t days) {
  // 1970-01-01 was a Thursday (3, counting Monday as 0).
  return static_cast<int>(((days % 7) + 7 + 3) % 7);
}

int daysInMonth(const int year, const int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 30;
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return leap ? 29 : 28;
  }
  return kDays[month - 1];
}

// ------------------------------------------------------------------ helpers

namespace {

constexpr size_t kMaxLogicalLine = 1200;
constexpr size_t kMaxTitleBytes = 100;
constexpr size_t kMaxExdates = 64;
constexpr size_t kMaxOverrides = 400;
constexpr int kMaxAllDaySpan = 31;
constexpr int kMaxIterations = 40000;

int32_t floorDiv(const int32_t a, const int32_t b) {
  int32_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

bool allDigits(const char* p, const size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (p[i] < '0' || p[i] > '9') return false;
  }
  return true;
}

int readInt(const char* p, const size_t n) {
  int value = 0;
  for (size_t i = 0; i < n; ++i) value = value * 10 + (p[i] - '0');
  return value;
}

std::string upper(std::string s) {
  for (auto& c : s) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  }
  return s;
}

std::string trim(const std::string& s) {
  size_t a = 0;
  size_t b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
  return s.substr(a, b - a);
}

struct Property {
  std::string name;    // upper-case
  std::string params;  // upper-case, everything between the first ';' and the ':'
  std::string value;
};

// NAME;PARAM=VALUE:VALUE. A colon inside a quoted parameter value is skipped.
bool splitProperty(const std::string& line, Property& out) {
  bool quoted = false;
  size_t colon = std::string::npos;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '"') quoted = !quoted;
    if (line[i] == ':' && !quoted) {
      colon = i;
      break;
    }
  }
  if (colon == std::string::npos) return false;
  const std::string head = line.substr(0, colon);
  const size_t semi = head.find(';');
  out.name = upper(semi == std::string::npos ? head : head.substr(0, semi));
  out.params = semi == std::string::npos ? std::string() : upper(head.substr(semi + 1));
  out.value = line.substr(colon + 1);
  return !out.name.empty();
}

bool hasDateValueParam(const std::string& params) {
  const size_t at = params.find("VALUE=DATE");
  if (at == std::string::npos) return false;
  const size_t end = at + std::strlen("VALUE=DATE");
  return end >= params.size() || params[end] == ';';
}

struct ParsedTime {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
  bool utc = false;
  bool dateOnly = false;
};

bool parseDateTime(const std::string& raw, const bool valueIsDate, ParsedTime& out) {
  const std::string v = trim(raw);
  if (v.size() < 8 || !allDigits(v.data(), 8)) return false;
  out.year = readInt(v.data(), 4);
  out.month = readInt(v.data() + 4, 2);
  out.day = readInt(v.data() + 6, 2);
  if (out.month < 1 || out.month > 12 || out.day < 1 || out.day > 31 || out.year < 1970 || out.year > 2200) {
    return false;
  }
  if (v.size() == 8 || valueIsDate) {
    out.dateOnly = true;
    return true;
  }
  if (v.size() < 15 || v[8] != 'T' || !allDigits(v.data() + 9, 4)) return false;
  out.hour = readInt(v.data() + 9, 2);
  out.minute = readInt(v.data() + 11, 2);
  if (out.hour > 23 || out.minute > 59) return false;
  out.utc = v.back() == 'Z' || v.back() == 'z';
  return true;
}

// Local date and minute for a parsed time.
void toLocal(const ParsedTime& t, const int utcOffsetMinutes, int32_t& day, uint16_t& minute) {
  const int32_t base = daysFromCivil(t.year, t.month, t.day);
  if (t.dateOnly) {
    day = base;
    minute = kNoTime;
    return;
  }
  int32_t total = t.hour * 60 + t.minute;
  if (t.utc) total += utcOffsetMinutes;
  day = base + floorDiv(total, 1440);
  int32_t m = total % 1440;
  if (m < 0) m += 1440;
  minute = static_cast<uint16_t>(m);
}

std::string unescapeText(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      const char n = s[i + 1];
      if (n == 'n' || n == 'N') {
        out.push_back(' ');
      } else {
        out.push_back(n);  // \, \; \\ and anything else
      }
      ++i;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

std::string truncateUtf8(const std::string& s, const size_t maxBytes) {
  if (s.size() <= maxBytes) return s;
  size_t cut = maxBytes;
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
  return s.substr(0, cut);
}

uint32_t fnv1a(const std::string& s) {
  uint32_t hash = 2166136261u;
  for (const unsigned char c : s) {
    hash ^= c;
    hash *= 16777619u;
  }
  return hash;
}

// "PT1H30M", "P1D", "P2W". Returns minutes, or -1 if it isn't one.
int parseDurationMinutes(const std::string& raw) {
  const std::string v = trim(raw);
  size_t i = 0;
  if (i < v.size() && (v[i] == '+' || v[i] == '-')) {
    if (v[i] == '-') return -1;
    ++i;
  }
  if (i >= v.size() || v[i] != 'P') return -1;
  ++i;
  bool inTime = false;
  int minutes = 0;
  bool any = false;
  while (i < v.size()) {
    if (v[i] == 'T') {
      inTime = true;
      ++i;
      continue;
    }
    int number = 0;
    bool digits = false;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
      number = number * 10 + (v[i] - '0');
      digits = true;
      ++i;
    }
    if (!digits || i >= v.size()) return -1;
    const char unit = v[i++];
    any = true;
    if (unit == 'W') minutes += number * 7 * 1440;
    else if (unit == 'D') minutes += number * 1440;
    else if (unit == 'H' && inTime) minutes += number * 60;
    else if (unit == 'M' && inTime) minutes += number;
    else if (unit == 'S' && inTime) minutes += number / 60;
    else return -1;
  }
  return any ? minutes : -1;
}

// 0 = Monday ... 6 = Sunday, or -1.
int weekdayCode(const std::string& two) {
  static const char* const kCodes[] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
  for (int i = 0; i < 7; ++i) {
    if (two == kCodes[i]) return i;
  }
  return -1;
}

// The day of the month of the nth given weekday (n > 0 from the start, n < 0
// from the end), or 0 if the month has no such day.
int nthWeekdayOfMonth(const int year, const int month, const int weekday, const int ordinal) {
  const int dim = daysInMonth(year, month);
  const int first = weekdayFromDays(daysFromCivil(year, month, 1));
  const int firstMatch = 1 + ((weekday - first + 7) % 7);
  if (ordinal > 0) {
    const int dom = firstMatch + 7 * (ordinal - 1);
    return dom <= dim ? dom : 0;
  }
  const int lastMatch = firstMatch + 7 * ((dim - firstMatch) / 7);
  const int dom = lastMatch + 7 * (ordinal + 1);
  return dom >= 1 ? dom : 0;
}

}  // namespace

// ------------------------------------------------------------- Calendar

const char* Calendar::title(const Occurrence& occurrence) const {
  if (occurrence.titleOffset >= titles.size()) return "";
  return titles.c_str() + occurrence.titleOffset;
}

uint32_t Calendar::monthMask(const int year, const int month) const {
  const int32_t first = daysFromCivil(year, month, 1);
  const int32_t last = first + daysInMonth(year, month) - 1;
  auto it = std::lower_bound(occurrences.begin(), occurrences.end(), first,
                             [](const Occurrence& o, const int32_t day) { return o.day < day; });
  uint32_t mask = 0;
  for (; it != occurrences.end() && it->day <= last; ++it) mask |= 1u << (it->day - first);
  return mask;
}

void Calendar::eventsOn(const int32_t day, std::vector<size_t>& out) const {
  out.clear();
  auto it = std::lower_bound(occurrences.begin(), occurrences.end(), day,
                             [](const Occurrence& o, const int32_t d) { return o.day < d; });
  for (; it != occurrences.end() && it->day == day; ++it) out.push_back(static_cast<size_t>(it - occurrences.begin()));
}

void Calendar::upcoming(const int32_t fromDay, const size_t limit, std::vector<size_t>& out) const {
  out.clear();
  auto it = std::lower_bound(occurrences.begin(), occurrences.end(), fromDay,
                             [](const Occurrence& o, const int32_t d) { return o.day < d; });
  for (; it != occurrences.end() && out.size() < limit; ++it) out.push_back(static_cast<size_t>(it - occurrences.begin()));
}

// --------------------------------------------------------------- Parser

struct Parser::Event {
  std::string summary;
  bool hasStart = false;
  bool allDay = false;
  int32_t startDay = 0;
  uint16_t startMinute = kNoTime;
  bool hasEnd = false;
  int32_t endDay = 0;
  uint16_t endMinute = kNoTime;
  int durationMinutes = -1;
  std::string rrule;
  std::vector<int32_t> exdates;
  bool hasRecurrenceId = false;
  int32_t recurrenceDay = 0;
  uint32_t uid = 0;
  bool cancelled = false;
};

struct Parser::Work {
  Occurrence occurrence;
  uint32_t uid = 0;
  bool isOverride = false;
};

struct Parser::Rule {
  enum class Freq : uint8_t { None, Daily, Weekly, Monthly, Yearly };
  struct Day {
    int ordinal;  // 0 = every, +n = nth from the start, -n = nth from the end
    int weekday;
  };
  Freq freq = Freq::None;
  int interval = 1;
  int count = 0;  // 0 = no limit
  int32_t untilDay = INT32_MAX;
  std::vector<Day> byDay;
  std::vector<int> byMonthDay;
  std::vector<int> byMonth;
  int weekStart = 0;
};

namespace {

// Splits "A=b;C=d" into pairs.
template <typename Fn>
void forEachRuleField(const std::string& rule, Fn fn) {
  size_t pos = 0;
  while (pos <= rule.size()) {
    size_t end = rule.find(';', pos);
    if (end == std::string::npos) end = rule.size();
    const std::string field = rule.substr(pos, end - pos);
    const size_t eq = field.find('=');
    if (eq != std::string::npos) fn(upper(field.substr(0, eq)), field.substr(eq + 1));
    pos = end + 1;
  }
}

template <typename Fn>
void forEachListItem(const std::string& list, Fn fn) {
  size_t pos = 0;
  while (pos <= list.size()) {
    size_t end = list.find(',', pos);
    if (end == std::string::npos) end = list.size();
    if (end > pos) fn(list.substr(pos, end - pos));
    pos = end + 1;
  }
}

}  // namespace

Parser::Parser(const ParseOptions& options) : options_(options) {
  work_.reserve(std::min<size_t>(options.maxOccurrences, 512));
  titles_.reserve(std::min<size_t>(options.maxTitleBytes, 4096));
}

Parser::~Parser() { delete current_; }

void Parser::feed(const uint8_t* data, const size_t length) {
  for (size_t i = 0; i < length; ++i) {
    const char c = static_cast<char>(data[i]);
    if (c == '\n') {
      endPhysicalLine();
    } else if (physical_.size() < kMaxLogicalLine) {
      physical_.push_back(c);
    }
  }
}

// A physical line that starts with a space or tab continues the previous one.
void Parser::endPhysicalLine() {
  if (!physical_.empty() && physical_.back() == '\r') physical_.pop_back();
  if (!physical_.empty() && (physical_[0] == ' ' || physical_[0] == '\t')) {
    if (logical_.size() < kMaxLogicalLine) logical_.append(physical_, 1, std::string::npos);
  } else {
    if (!logical_.empty()) processLine(logical_);
    logical_ = physical_;
  }
  physical_.clear();
}

void Parser::processLine(const std::string& line) {
  Property p;
  if (!splitProperty(line, p)) return;
  const std::string value = trim(p.value);

  if (p.name == "BEGIN") {
    const std::string kind = upper(value);
    if (kind == "VCALENDAR") {
      sawCalendar_ = true;
    } else if (kind == "VEVENT") {
      inEvent_ = true;
      alarmDepth_ = 0;
      delete current_;
      current_ = new Event();
    } else if (kind == "VALARM" && inEvent_) {
      ++alarmDepth_;
    }
    return;
  }
  if (p.name == "END") {
    const std::string kind = upper(value);
    if (kind == "VEVENT" && inEvent_) {
      finishEvent();
      inEvent_ = false;
      delete current_;
      current_ = nullptr;
    } else if (kind == "VALARM" && inEvent_ && alarmDepth_ > 0) {
      --alarmDepth_;
    }
    return;
  }
  if (!inEvent_ || alarmDepth_ > 0 || current_ == nullptr) return;
  Event& e = *current_;

  if (p.name == "SUMMARY") {
    e.summary = truncateUtf8(trim(unescapeText(p.value)), kMaxTitleBytes);
  } else if (p.name == "DTSTART") {
    ParsedTime t;
    if (parseDateTime(p.value, hasDateValueParam(p.params), t)) {
      toLocal(t, options_.utcOffsetMinutes, e.startDay, e.startMinute);
      e.allDay = t.dateOnly;
      e.hasStart = true;
    }
  } else if (p.name == "DTEND") {
    ParsedTime t;
    if (parseDateTime(p.value, hasDateValueParam(p.params), t)) {
      toLocal(t, options_.utcOffsetMinutes, e.endDay, e.endMinute);
      e.hasEnd = true;
    }
  } else if (p.name == "DURATION") {
    e.durationMinutes = parseDurationMinutes(p.value);
  } else if (p.name == "RRULE") {
    e.rrule = value;
  } else if (p.name == "EXDATE") {
    const bool dateValue = hasDateValueParam(p.params);
    forEachListItem(value, [&](const std::string& item) {
      ParsedTime t;
      if (e.exdates.size() < kMaxExdates && parseDateTime(item, dateValue, t)) {
        int32_t day = 0;
        uint16_t minute = 0;
        toLocal(t, options_.utcOffsetMinutes, day, minute);
        e.exdates.push_back(day);
      }
    });
  } else if (p.name == "RECURRENCE-ID") {
    ParsedTime t;
    if (parseDateTime(p.value, hasDateValueParam(p.params), t)) {
      uint16_t minute = 0;
      toLocal(t, options_.utcOffsetMinutes, e.recurrenceDay, minute);
      e.hasRecurrenceId = true;
    }
  } else if (p.name == "UID") {
    e.uid = fnv1a(value);
  } else if (p.name == "STATUS") {
    e.cancelled = upper(value) == "CANCELLED";
  }
}

void Parser::finishEvent() {
  Event& e = *current_;
  ++eventsSeen_;
  // A moved or cancelled instance hides the original occurrence it replaces,
  // whether or not it is itself inside the window.
  if (e.hasRecurrenceId && overrides_.size() < kMaxOverrides) overrides_.emplace_back(e.uid, e.recurrenceDay);
  if (!e.hasStart || e.cancelled) return;

  Rule rule;
  bool hasRule = false;
  if (!e.rrule.empty() && !e.hasRecurrenceId) {
    forEachRuleField(e.rrule, [&](const std::string& key, const std::string& val) {
      if (key == "FREQ") {
        const std::string f = upper(val);
        rule.freq = f == "DAILY"     ? Rule::Freq::Daily
                    : f == "WEEKLY"  ? Rule::Freq::Weekly
                    : f == "MONTHLY" ? Rule::Freq::Monthly
                    : f == "YEARLY"  ? Rule::Freq::Yearly
                                     : Rule::Freq::None;
      } else if (key == "INTERVAL") {
        rule.interval = std::max(1, std::min(1000, atoi(val.c_str())));
      } else if (key == "COUNT") {
        rule.count = std::max(0, atoi(val.c_str()));
      } else if (key == "UNTIL") {
        ParsedTime t;
        if (parseDateTime(val, false, t)) {
          uint16_t minute = 0;
          toLocal(t, options_.utcOffsetMinutes, rule.untilDay, minute);
        }
      } else if (key == "BYDAY") {
        forEachListItem(val, [&](const std::string& item) {
          if (item.size() < 2) return;
          const int wd = weekdayCode(upper(item.substr(item.size() - 2)));
          if (wd < 0) return;
          int ordinal = 0;
          if (item.size() > 2) ordinal = atoi(item.substr(0, item.size() - 2).c_str());
          rule.byDay.push_back({ordinal, wd});
        });
      } else if (key == "BYMONTHDAY") {
        forEachListItem(val, [&](const std::string& item) { rule.byMonthDay.push_back(atoi(item.c_str())); });
      } else if (key == "BYMONTH") {
        forEachListItem(val, [&](const std::string& item) {
          const int m = atoi(item.c_str());
          if (m >= 1 && m <= 12) rule.byMonth.push_back(m);
        });
        std::sort(rule.byMonth.begin(), rule.byMonth.end());
      } else if (key == "WKST") {
        const int wd = weekdayCode(upper(val));
        if (wd >= 0) rule.weekStart = wd;
      }
    });
    hasRule = rule.freq != Rule::Freq::None;
  }
  expand(e, hasRule ? &rule : nullptr);
}

// Adds the title to the pool once and returns its offset.
bool Parser::titleFor(const Event& event, uint16_t& offset) {
  const std::string title = event.summary.empty() ? std::string("(No title)") : event.summary;
  for (const uint16_t existing : titleOffsets_) {
    if (title == titles_.c_str() + existing) {
      offset = existing;
      return true;
    }
  }
  if (titles_.size() + title.size() + 1 > options_.maxTitleBytes || titles_.size() > 60000) {
    truncated_ = true;
    return false;
  }
  offset = static_cast<uint16_t>(titles_.size());
  titles_.append(title);
  titles_.push_back('\0');
  titleOffsets_.push_back(offset);
  return true;
}

// `part` is 0 for the day an event starts on, then 1, 2... for each further
// day it covers (`extraDays` of them). `totalEnd` is the end in minutes after
// the start day's midnight, or -1 if the event has no known end.
void Parser::emit(const Event& event, const int32_t day, const uint16_t titleOffset, const int part,
                  const int extraDays, const int totalEnd) {
  if (day < options_.windowStartDay || day > options_.windowEndDay) return;
  if (work_.size() >= options_.maxOccurrences) {
    truncated_ = true;
    return;
  }
  Work w;
  w.occurrence.day = day;
  w.occurrence.titleOffset = titleOffset;
  if (!event.allDay) {
    const bool last = part >= extraDays;
    if (part == 0) {
      w.occurrence.startMinute = event.startMinute;
    } else {
      w.occurrence.startMinute = 0;
      w.occurrence.flags |= kFromPreviousDay;
    }
    if (!last) {
      w.occurrence.endMinute = 1440;
      w.occurrence.flags |= kToNextDay;
    } else if (totalEnd > 0) {
      w.occurrence.endMinute = static_cast<uint16_t>(totalEnd - extraDays * 1440);
    }
  }
  w.uid = event.uid;
  w.isOverride = event.hasRecurrenceId;
  work_.push_back(w);
}

void Parser::expand(const Event& event, const Rule* rule) {
  // How many days an all-day event covers; timed events sit on their start day.
  int span = 1;
  int extraDays = 0;  // timed events: further days past the start day that it runs into
  int totalEnd = -1;
  if (event.allDay) {
    if (event.hasEnd) span = static_cast<int>(event.endDay - event.startDay);
    else if (event.durationMinutes > 0) span = event.durationMinutes / 1440;
    span = std::max(1, std::min(kMaxAllDaySpan, span));
  } else {
    if (event.hasEnd && event.endMinute != kNoTime) {
      totalEnd = static_cast<int>(event.endDay - event.startDay) * 1440 + event.endMinute;
    } else if (event.durationMinutes >= 0) {
      totalEnd = event.startMinute + event.durationMinutes;
    }
    if (totalEnd <= static_cast<int>(event.startMinute)) totalEnd = -1;
    // Ending exactly at midnight stays on the start day.
    if (totalEnd > 1440) extraDays = std::min(kMaxAllDaySpan, (totalEnd - 1) / 1440);
    span = 1 + extraDays;
  }

  uint16_t titleOffset = 0;
  bool haveTitle = false;
  // Interns the title only once something is actually going to be stored, so
  // events outside the window never use up title space.
  auto emitSpan = [&](const int32_t startDay) {
    if (startDay > options_.windowEndDay || startDay + span - 1 < options_.windowStartDay) return;
    if (!haveTitle) {
      if (!titleFor(event, titleOffset)) return;
      haveTitle = true;
    }
    for (int i = 0; i < span; ++i) emit(event, startDay + i, titleOffset, i, extraDays, totalEnd);
  };

  if (rule == nullptr) {
    emitSpan(event.startDay);
    return;
  }

  int produced = 0;
  // Returns false once nothing later can matter, ending the generation.
  auto add = [&](const int32_t day) -> bool {
    if (day < event.startDay) return true;
    if (day > rule->untilDay || day > options_.windowEndDay) return false;
    ++produced;
    if (rule->count > 0 && produced > rule->count) return false;
    if (std::find(event.exdates.begin(), event.exdates.end(), day) != event.exdates.end()) return true;
    emitSpan(day);
    return true;
  };

  int startYear = 0, startMonth = 0, startDom = 0;
  civilFromDays(event.startDay, startYear, startMonth, startDom);

  // Days of one month that the rule selects, ascending.
  auto monthDays = [&](const int year, const int month, std::vector<int32_t>& out) {
    out.clear();
    const int dim = daysInMonth(year, month);
    std::vector<int> doms;
    if (!rule->byDay.empty()) {
      const int first = weekdayFromDays(daysFromCivil(year, month, 1));
      for (const auto& item : rule->byDay) {
        if (item.ordinal != 0) {
          const int dom = nthWeekdayOfMonth(year, month, item.weekday, item.ordinal);
          if (dom > 0) doms.push_back(dom);
        } else {
          for (int dom = 1 + ((item.weekday - first + 7) % 7); dom <= dim; dom += 7) doms.push_back(dom);
        }
      }
    } else if (!rule->byMonthDay.empty()) {
      for (const int v : rule->byMonthDay) {
        const int dom = v > 0 ? v : dim + v + 1;
        if (dom >= 1 && dom <= dim) doms.push_back(dom);
      }
    } else if (startDom <= dim) {
      doms.push_back(startDom);
    }
    std::sort(doms.begin(), doms.end());
    doms.erase(std::unique(doms.begin(), doms.end()), doms.end());
    for (const int dom : doms) out.push_back(daysFromCivil(year, month, dom));
  };

  int guard = 0;
  switch (rule->freq) {
    case Rule::Freq::Daily: {
      for (int32_t day = event.startDay; guard++ < kMaxIterations; day += rule->interval) {
        if (!rule->byDay.empty()) {
          const int wd = weekdayFromDays(day);
          const bool match = std::any_of(rule->byDay.begin(), rule->byDay.end(),
                                         [wd](const Rule::Day& d) { return d.weekday == wd; });
          if (!match) {
            if (day > options_.windowEndDay) break;
            continue;
          }
        }
        if (!add(day)) break;
      }
      break;
    }
    case Rule::Freq::Weekly: {
      std::vector<int> weekdays;
      for (const auto& d : rule->byDay) weekdays.push_back(d.weekday);
      if (weekdays.empty()) weekdays.push_back(weekdayFromDays(event.startDay));
      std::sort(weekdays.begin(), weekdays.end(), [&](const int a, const int b) {
        return (a - rule->weekStart + 7) % 7 < (b - rule->weekStart + 7) % 7;
      });
      const int32_t firstWeek = event.startDay - ((weekdayFromDays(event.startDay) - rule->weekStart + 7) % 7);
      bool done = false;
      for (int32_t week = firstWeek; !done && guard++ < kMaxIterations; week += 7 * rule->interval) {
        for (const int wd : weekdays) {
          if (!add(week + ((wd - rule->weekStart + 7) % 7))) {
            done = true;
            break;
          }
        }
      }
      break;
    }
    case Rule::Freq::Monthly: {
      std::vector<int32_t> days;
      int year = startYear;
      int month = startMonth;
      for (bool done = false; !done && guard++ < 1200;) {
        monthDays(year, month, days);
        for (const int32_t day : days) {
          if (!add(day)) {
            done = true;
            break;
          }
        }
        month += rule->interval;
        while (month > 12) {
          month -= 12;
          ++year;
        }
      }
      break;
    }
    case Rule::Freq::Yearly: {
      std::vector<int32_t> days;
      std::vector<int> months = rule->byMonth;
      if (months.empty()) months.push_back(startMonth);
      bool done = false;
      for (int year = startYear; !done && guard++ < 300; year += rule->interval) {
        for (const int month : months) {
          monthDays(year, month, days);
          for (const int32_t day : days) {
            if (!add(day)) {
              done = true;
              break;
            }
          }
          if (done) break;
        }
      }
      break;
    }
    case Rule::Freq::None:
      break;
  }
}

Calendar Parser::finish() {
  if (!physical_.empty()) endPhysicalLine();
  if (!logical_.empty()) {
    processLine(logical_);
    logical_.clear();
  }

  Calendar calendar;
  calendar.windowStartDay = options_.windowStartDay;
  calendar.windowEndDay = options_.windowEndDay;
  calendar.truncated = truncated_;

  for (const Work& w : work_) {
    if (!w.isOverride) {
      const bool replaced = std::any_of(overrides_.begin(), overrides_.end(), [&](const auto& key) {
        return key.first == w.uid && key.second == w.occurrence.day;
      });
      if (replaced) continue;
    }
    calendar.occurrences.push_back(w.occurrence);
  }
  std::stable_sort(calendar.occurrences.begin(), calendar.occurrences.end(),
                   [](const Occurrence& a, const Occurrence& b) {
                     if (a.day != b.day) return a.day < b.day;
                     const uint32_t ka = a.allDay() ? 0u : a.startMinute + 1u;
                     const uint32_t kb = b.allDay() ? 0u : b.startMinute + 1u;
                     return ka < kb;
                   });
  calendar.titles = std::move(titles_);
  return calendar;
}

// -------------------------------------------------------------- storage

namespace {
constexpr char kMagic[4] = {'R', 'C', 'A', 'L'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 24;
constexpr size_t kRecordSize = 12;

void putU16(std::string& s, const uint16_t v) {
  s.push_back(static_cast<char>(v & 0xFF));
  s.push_back(static_cast<char>(v >> 8));
}
void putU32(std::string& s, const uint32_t v) {
  for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
uint16_t getU16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t getU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

std::string serialize(const Calendar& calendar) {
  std::string out;
  out.reserve(kHeaderSize + calendar.titles.size() + calendar.occurrences.size() * kRecordSize);
  out.append(kMagic, 4);
  out.push_back(static_cast<char>(kVersion));
  out.push_back(static_cast<char>(calendar.truncated ? 1 : 0));
  putU16(out, static_cast<uint16_t>(calendar.occurrences.size()));
  putU32(out, static_cast<uint32_t>(calendar.syncedDay));
  putU32(out, static_cast<uint32_t>(calendar.windowStartDay));
  putU32(out, static_cast<uint32_t>(calendar.windowEndDay));
  putU32(out, static_cast<uint32_t>(calendar.titles.size()));
  out.append(calendar.titles);
  for (const Occurrence& o : calendar.occurrences) {
    putU32(out, static_cast<uint32_t>(o.day));
    putU16(out, o.startMinute);
    putU16(out, o.endMinute);
    putU16(out, o.titleOffset);
    putU16(out, o.flags);
  }
  return out;
}

bool deserialize(const uint8_t* data, const size_t length, Calendar& out) {
  if (data == nullptr || length < kHeaderSize || std::memcmp(data, kMagic, 4) != 0 || data[4] != kVersion) return false;
  const uint16_t count = getU16(data + 6);
  const uint32_t titleBytes = getU32(data + 20);
  if (titleBytes > 65535 || kHeaderSize + titleBytes + static_cast<size_t>(count) * kRecordSize != length) return false;

  Calendar c;
  c.truncated = (data[5] & 1) != 0;
  c.syncedDay = static_cast<int32_t>(getU32(data + 8));
  c.windowStartDay = static_cast<int32_t>(getU32(data + 12));
  c.windowEndDay = static_cast<int32_t>(getU32(data + 16));
  c.titles.assign(reinterpret_cast<const char*>(data + kHeaderSize), titleBytes);
  c.occurrences.reserve(count);
  const uint8_t* record = data + kHeaderSize + titleBytes;
  for (uint16_t i = 0; i < count; ++i, record += kRecordSize) {
    Occurrence o;
    o.day = static_cast<int32_t>(getU32(record));
    o.startMinute = getU16(record + 4);
    o.endMinute = getU16(record + 6);
    o.titleOffset = getU16(record + 8);
    o.flags = static_cast<uint8_t>(getU16(record + 10) & (kFromPreviousDay | kToNextDay));
    if (o.titleOffset >= titleBytes && titleBytes > 0) return false;
    c.occurrences.push_back(o);
  }
  out = std::move(c);
  return true;
}

}  // namespace ics
