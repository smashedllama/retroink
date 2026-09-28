#include "DeskDate.h"

#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"

namespace {
constexpr char kDeskDatePath[] = "/.crosspoint/desk_date.bin";
constexpr uint8_t kDeskDateVersion = 1;
// version, year lo/hi, month, day, hour, minute, zoneQ.
constexpr size_t kDeskDateSize = 8;

constexpr const char* kMonthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr const char* kFullMonthNames[] = {"January", "February", "March",     "April",   "May",      "June",
                                           "July",    "August",   "September", "October", "November", "December"};

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's
// days_from_civil), and back. Plenty for 1970-2100 without any time library.
int32_t daysFromCivil(int y, const int m, const int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void civilFromDays(int32_t z, int& y, int& m, int& d) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const int doe = z - era * 146097;
  const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y = yoe + era * 400 + (m <= 2);
}

int zoneMinutes(const uint8_t zoneQ) { return (static_cast<int>(std::min(zoneQ, DeskDate::kMaxZoneQ)) - 48) * 15; }

// Shifts a date/time by a number of minutes, rolling the date as needed.
void shiftMinutes(int& y, int& m, int& d, int& hour, int& minute, const int delta) {
  int32_t days = daysFromCivil(y, m, d);
  int32_t total = hour * 60 + minute + delta;
  while (total < 0) {
    total += 1440;
    --days;
  }
  while (total >= 1440) {
    total -= 1440;
    ++days;
  }
  civilFromDays(days, y, m, d);
  hour = total / 60;
  minute = total % 60;
}

char dateSeparator() {
  switch (SETTINGS.dateSeparator) {
    case CrossPointSettings::DATE_SEPARATOR_PERIOD:
      return '.';
    case CrossPointSettings::DATE_SEPARATOR_HYPHEN:
      return '-';
    default:
      return '/';
  }
}
}  // namespace

namespace DeskDate {

int daysInMonth(const int year, const int month) {
  static constexpr uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 31;
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return leap ? 29 : 28;
  }
  return kDays[month - 1];
}

void normalize(DeskDateTime& value) {
  value.year = std::clamp<uint16_t>(value.year, kMinYear, kMaxYear);
  value.month = std::clamp<uint8_t>(value.month, 1, 12);
  value.day = std::clamp<uint8_t>(value.day, 1, static_cast<uint8_t>(daysInMonth(value.year, value.month)));
  value.hour = std::min<uint8_t>(value.hour, 23);
  value.minute = std::min<uint8_t>(value.minute, 59);
  value.zoneQ = std::min(value.zoneQ, kMaxZoneQ);
}

bool loadSaved(DeskDateTime& out) {
  FsFile f;
  if (!Storage.openFileForRead("DDT", kDeskDatePath, f)) return false;
  uint8_t data[kDeskDateSize] = {};
  const bool sizeOk = f.fileSize() == kDeskDateSize;
  const int n = sizeOk ? f.read(data, kDeskDateSize) : 0;
  f.close();
  if (!sizeOk || n != static_cast<int>(kDeskDateSize) || data[0] != kDeskDateVersion) return false;
  out.year = static_cast<uint16_t>(data[1] | (data[2] << 8));
  out.month = data[3];
  out.day = data[4];
  out.hour = data[5];
  out.minute = data[6];
  out.zoneQ = data[7];
  normalize(out);
  return true;
}

void save(const DeskDateTime& value) {
  DeskDateTime v = value;
  normalize(v);
  const uint8_t data[kDeskDateSize] = {kDeskDateVersion,
                                       static_cast<uint8_t>(v.year & 0xFF),
                                       static_cast<uint8_t>(v.year >> 8),
                                       v.month,
                                       v.day,
                                       v.hour,
                                       v.minute,
                                       v.zoneQ};
  FsFile f;
  if (!Storage.openFileForWrite("DDT", kDeskDatePath, f)) {
    LOG_ERR("DDT", "Could not open desk date for write");
    return;
  }
  const size_t written = f.write(data, kDeskDateSize);
  f.close();
  if (written != kDeskDateSize) LOG_ERR("DDT", "Short write saving desk date");
}

DeskDateTime fallback() {
  DeskDateTime value;
  DeskDateTime saved;
  value.zoneQ = loadSaved(saved) ? saved.zoneQ : std::min(SETTINGS.clockUtcOffsetQ, kMaxZoneQ);
  return value;
}

bool current(DeskDateTime& out) {
  uint16_t year;
  uint8_t month, day, hour, minute;
  if (halClock.isAvailable() && SETTINGS.clockDateHasBeenSynced && halClock.getDateTime(year, month, day, hour, minute)) {
    // With a clock, "today" is always the device's own local date; a date
    // picked to look at is only kept for clockless hardware.
    out.zoneQ = std::min(SETTINGS.clockUtcOffsetQ, kMaxZoneQ);
    int y = year, m = month, d = day, h = hour, mi = minute;
    shiftMinutes(y, m, d, h, mi, zoneMinutes(out.zoneQ));
    out.year = static_cast<uint16_t>(y);
    out.month = static_cast<uint8_t>(m);
    out.day = static_cast<uint8_t>(d);
    out.hour = static_cast<uint8_t>(h);
    out.minute = static_cast<uint8_t>(mi);
    normalize(out);
    return true;
  }
  return loadSaved(out);
}

void toUtc(const DeskDateTime& value, uint16_t& year, uint8_t& month, uint8_t& day, uint8_t& hour,
           uint8_t& minute) {
  int y = value.year, m = value.month, d = value.day, h = value.hour, mi = value.minute;
  shiftMinutes(y, m, d, h, mi, -zoneMinutes(value.zoneQ));
  year = static_cast<uint16_t>(y);
  month = static_cast<uint8_t>(m);
  day = static_cast<uint8_t>(d);
  hour = static_cast<uint8_t>(h);
  minute = static_cast<uint8_t>(mi);
}

void formatDate(const DeskDateTime& value, char* buf, const size_t len) {
  const unsigned d = value.day, m = value.month, y = value.year;
  const char sep = dateSeparator();
  const char* shortMonth = kMonthNames[(m - 1) % 12];
  const char* fullMonth = kFullMonthNames[(m - 1) % 12];
  switch (SETTINGS.dateFormat) {
    case CrossPointSettings::DATE_FORMAT_DAY_MONTH_YEAR_LONG:
      snprintf(buf, len, "%02u %s %u", d, shortMonth, y);
      break;
    case CrossPointSettings::DATE_FORMAT_MONTH_DAY_YEAR_NUMERIC:
      snprintf(buf, len, "%02u%c%02u%c%u", m, sep, d, sep, y);
      break;
    case CrossPointSettings::DATE_FORMAT_DAY_MONTH_YEAR_NUMERIC:
      snprintf(buf, len, "%02u%c%02u%c%u", d, sep, m, sep, y);
      break;
    case CrossPointSettings::DATE_FORMAT_YEAR_MONTH_DAY_NUMERIC:
      snprintf(buf, len, "%u%c%02u%c%02u", y, sep, m, sep, d);
      break;
    case CrossPointSettings::DATE_FORMAT_MONTH_DAY_NUMERIC:
      snprintf(buf, len, "%02u%c%02u", m, sep, d);
      break;
    case CrossPointSettings::DATE_FORMAT_DAY_MONTH_NUMERIC:
      snprintf(buf, len, "%02u%c%02u", d, sep, m);
      break;
    case CrossPointSettings::DATE_FORMAT_MONTH_DAY_LONG:
      snprintf(buf, len, "%s %02u", fullMonth, d);
      break;
    case CrossPointSettings::DATE_FORMAT_DAY_MONTH_LONG:
      snprintf(buf, len, "%02u %s", d, fullMonth);
      break;
    default:
      snprintf(buf, len, "%s %02u, %u", shortMonth, d, y);
      break;
  }
}

void formatTime(const DeskDateTime& value, char* buf, const size_t len) {
  if (SETTINGS.clockFormat == 1) {
    const unsigned h12 = value.hour % 12 == 0 ? 12 : value.hour % 12;
    snprintf(buf, len, "%u:%02u %s", h12, static_cast<unsigned>(value.minute), value.hour < 12 ? "AM" : "PM");
  } else {
    snprintf(buf, len, "%02u:%02u", static_cast<unsigned>(value.hour), static_cast<unsigned>(value.minute));
  }
}

void formatZone(const uint8_t zoneQ, char* buf, const size_t len) {
  const int minutes = zoneMinutes(zoneQ);
  if (minutes == 0) {
    snprintf(buf, len, "UTC");
    return;
  }
  const int absMinutes = minutes < 0 ? -minutes : minutes;
  const char sign = minutes < 0 ? '-' : '+';
  if (absMinutes % 60 == 0) {
    snprintf(buf, len, "UTC%c%d", sign, absMinutes / 60);
  } else {
    snprintf(buf, len, "UTC%c%d:%02d", sign, absMinutes / 60, absMinutes % 60);
  }
}

}  // namespace DeskDate
