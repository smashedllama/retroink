#pragma once

#include <cstddef>

// Small shared helper for the new desk-accessory screens (Moon Phase, Clock,
// Desk Calendar) that each need to format "today" using the user's clock
// settings (UTC offset, date format, separator) the same way the header
// clock does, without duplicating that formatting logic in each screen.

// Formats the current local date into buf using SETTINGS.dateFormat/dateSeparator
// and SETTINGS.clockUtcOffsetQ. Returns false if the RTC is unavailable or has
// never been synced.
bool formatLocalDate(char* buf, size_t len);
