#include "CalendarSync.h"

#include <HalStorage.h>
#include <Logging.h>

#include <strings.h>

#include <algorithm>
#include <cstring>

#include "CalendarFeedStore.h"
#include "CrossPointSettings.h"
#include "components/DeskDate.h"
#include "network/HttpDownloader.h"

namespace CalendarSync {
namespace {
constexpr char kPath[] = "/.crosspoint/calendar.bin";
constexpr char kTmpPath[] = "/.crosspoint/calendar.bin.tmp";
constexpr int kDaysBefore = 45;
constexpr int kDaysAfter = 400;

std::string normalizeUrl(std::string url) {
  while (!url.empty() && (url.front() == ' ' || url.front() == '\t')) url.erase(url.begin());
  while (!url.empty() && (url.back() == ' ' || url.back() == '\t' || url.back() == '\r' || url.back() == '\n')) {
    url.pop_back();
  }
  const std::string webcal = "webcal://";
  if (url.size() > webcal.size() && strncasecmp(url.c_str(), webcal.c_str(), webcal.size()) == 0) {
    url = "https://" + url.substr(webcal.size());
  }
  return url;
}
}  // namespace

const char* resultText(const Result result) {
  switch (result) {
    case Result::Ok:
      return "Calendar synced";
    case Result::NotConfigured:
      return "Calendar link isn't set up";
    case Result::NoDate:
      return "Set the date in Desk Calendar first";
    case Result::Network:
      return "Couldn't reach the calendar";
    case Result::NotCalendar:
      return "That link isn't a calendar feed";
    case Result::WriteFailed:
      return "Couldn't save to the SD card";
  }
  return "Calendar sync failed";
}

Result run(size_t* eventCount, bool* truncated) {
  const CalendarFeedConfig& cfg = CALENDAR_FEED.getConfig();
  const std::string url = normalizeUrl(cfg.url);
  if (!cfg.enabled || url.empty()) return Result::NotConfigured;

  DeskDateTime now;
  if (!DeskDate::current(now)) return Result::NoDate;
  const int32_t today = ics::daysFromCivil(now.year, now.month, now.day);

  ics::ParseOptions options;
  options.windowStartDay = today - kDaysBefore;
  options.windowEndDay = today + kDaysAfter;
  options.utcOffsetMinutes = (static_cast<int>(std::min(now.zoneQ, DeskDate::kMaxZoneQ)) - 48) * 15;
  ics::Parser parser(options);

  const auto error = HttpDownloader::streamUrl(url, [&parser](const uint8_t* data, const size_t len) {
    parser.feed(data, len);
    return true;
  });
  LOG_DBG("CAL", "Feed download result %d, events seen %u", static_cast<int>(error),
          static_cast<unsigned>(parser.eventsSeen()));
  if (error != HttpDownloader::OK) return Result::Network;
  if (!parser.sawCalendar()) return Result::NotCalendar;

  ics::Calendar calendar = parser.finish();
  calendar.syncedDay = today;
  const std::string blob = ics::serialize(calendar);

  Storage.mkdir("/.crosspoint");
  Storage.remove(kTmpPath);
  FsFile f;
  if (!Storage.openFileForWrite("CAL", kTmpPath, f)) return Result::WriteFailed;
  const size_t written = f.write(reinterpret_cast<const uint8_t*>(blob.data()), blob.size());
  f.close();
  if (written != blob.size()) {
    Storage.remove(kTmpPath);
    return Result::WriteFailed;
  }
  Storage.remove(kPath);
  if (!Storage.rename(kTmpPath, kPath)) return Result::WriteFailed;

  if (eventCount) *eventCount = calendar.occurrences.size();
  if (truncated) *truncated = calendar.truncated;
  LOG_DBG("CAL", "Stored %u entries (%u bytes)", static_cast<unsigned>(calendar.occurrences.size()),
          static_cast<unsigned>(blob.size()));
  return Result::Ok;
}

bool loadStored(ics::Calendar& out) {
  FsFile f;
  if (!Storage.openFileForRead("CAL", kPath, f)) return false;
  const size_t size = f.fileSize();
  if (size == 0 || size > 64 * 1024) {
    f.close();
    return false;
  }
  std::string blob(size, '\0');
  const int n = f.read(reinterpret_cast<uint8_t*>(blob.data()), size);
  f.close();
  if (n != static_cast<int>(size)) return false;
  return ics::deserialize(reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), out);
}

bool hasStored() { return Storage.exists(kPath); }

}  // namespace CalendarSync
