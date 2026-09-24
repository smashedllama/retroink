#include "ClockFormat.h"

#include <HalClock.h>

#include "CrossPointSettings.h"

namespace {
char dateSeparatorChar() {
  switch (SETTINGS.dateSeparator) {
    case CrossPointSettings::DATE_SEPARATOR_PERIOD:
      return '.';
    case CrossPointSettings::DATE_SEPARATOR_HYPHEN:
      return '-';
    case CrossPointSettings::DATE_SEPARATOR_SLASH:
    default:
      return '/';
  }
}
}  // namespace

bool formatLocalDate(char* buf, const size_t len) {
  if (!halClock.isAvailable()) return false;
  if (!SETTINGS.clockDateHasBeenSynced) return false;
#if defined(SIMULATOR) && !defined(CROSSPOINT_SIMULATOR_HAS_DATE_FORMAT)
  return halClock.formatDate(buf, len, SETTINGS.clockUtcOffsetQ);
#elif defined(SIMULATOR) && !defined(CROSSPOINT_SIMULATOR_HAS_DATE_SEPARATOR)
  if (!halClock.formatDate(buf, len, SETTINGS.clockUtcOffsetQ,
                           static_cast<HalClock::DateFormat>(SETTINGS.dateFormat))) {
    return false;
  }
  const char separator = dateSeparatorChar();
  if (separator != '/') {
    for (char* p = buf; *p != '\0'; ++p) {
      if (*p == '/') *p = separator;
    }
  }
  return true;
#else
  return halClock.formatDate(buf, len, SETTINGS.clockUtcOffsetQ, static_cast<HalClock::DateFormat>(SETTINGS.dateFormat),
                             dateSeparatorChar());
#endif
}
