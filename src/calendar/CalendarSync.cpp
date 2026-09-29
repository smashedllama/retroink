#include "CalendarSync.h"

#include <HalStorage.h>
#include <Logging.h>

#include <strings.h>

#include <Arduino.h>

#include <algorithm>
#include <memory>
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
      return "No calendar link set up yet";
    case Result::NoDate:
      return "Set the date first (Options)";
    case Result::Network:
      return "Couldn't reach the calendar";
    case Result::NotCalendar:
      return "That link isn't a calendar";
    case Result::WriteFailed:
      return "Couldn't save to the SD card";
  }
  return "Calendar sync failed";
}

namespace {
std::string g_detail;
}  // namespace

std::string lastFailureDetail() { return g_detail; }

Result run(size_t* eventCount, bool* truncated, const ProgressFn& progress) {
  g_detail.clear();
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

  // Feeds are often chunked, so the size is usually unknown up front. With no
  // size, the bar eases toward the end so it still shows movement.
  constexpr size_t kTypicalFeedBytes = 300 * 1024;
  size_t knownTotal = 0;
  size_t received = 0;
  int lastPercent = -1;
  uint32_t lastReportMs = 0;
  auto report = [&](const bool force) {
    if (!progress) return;
    int percent = knownTotal > 0 ? static_cast<int>(std::min<size_t>(99, received * 100 / knownTotal))
                                 : static_cast<int>(95 * received / (received + kTypicalFeedBytes));
    const uint32_t nowMs = millis();
    if (!force && (percent - lastPercent < 3 || nowMs - lastReportMs < 700)) return;
    lastPercent = percent;
    lastReportMs = nowMs;
    progress(percent);
  };

  // A second try over the other TLS stack, on a fresh parser, covers servers
  // the first stack cannot talk to.
  const HttpDownloader::Transport transports[] = {HttpDownloader::Transport::ESP_HTTP,
                                                  HttpDownloader::Transport::WOLFSSL};
  std::unique_ptr<ics::Parser> parser;
  HttpDownloader::DownloadError error = HttpDownloader::HTTP_ERROR;
  for (const auto transport : transports) {
    parser = std::make_unique<ics::Parser>(options);
    received = 0;
    knownTotal = 0;
    lastPercent = -1;
    report(true);
    error = HttpDownloader::streamUrl(
        url,
        [&](const uint8_t* data, const size_t len) {
          parser->feed(data, len);
          received += len;
          report(false);
          return true;
        },
        [&](const size_t, const size_t total) { knownTotal = total; }, "", "",
        HttpDownloader::DownloadOptions(false, false, nullptr, 0, transport));
    LOG_DBG("CAL", "Feed download (%s) result %d, %u bytes, %u events seen",
            transport == HttpDownloader::Transport::ESP_HTTP ? "esp" : "wolfssl", static_cast<int>(error),
            static_cast<unsigned>(received), static_cast<unsigned>(parser->eventsSeen()));
    if (error == HttpDownloader::OK) break;
    if (g_detail.empty()) g_detail = HttpDownloader::lastFailure();
  }
  if (error != HttpDownloader::OK) return Result::Network;
  g_detail.clear();
  if (!parser->sawCalendar()) return Result::NotCalendar;
  if (progress) progress(100);

  ics::Calendar calendar = parser->finish();
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
