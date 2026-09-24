#include "CalendarView.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "fontIds.h"
#include "themes/BaseTheme.h"

namespace CalendarView {

namespace {
bool isLeapYear(const int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int daysInMonth(const int year, const int month) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) return 29;
  return days[month - 1];
}

// Sakamoto's algorithm: 0 = Sunday .. 6 = Saturday.
int dayOfWeek(int year, const int month, const int day) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

// tr(id) is a macro that expands `id` to `StrId::id` -- it only works with a
// literal token, not a runtime-computed value, so these return StrId and
// callers go through I18n::getInstance().get() directly instead of tr().
StrId monthNameStrId(const int month) {
  static const StrId ids[] = {StrId::STR_MONTH_1, StrId::STR_MONTH_2,  StrId::STR_MONTH_3,  StrId::STR_MONTH_4,
                              StrId::STR_MONTH_5, StrId::STR_MONTH_6,  StrId::STR_MONTH_7,  StrId::STR_MONTH_8,
                              StrId::STR_MONTH_9, StrId::STR_MONTH_10, StrId::STR_MONTH_11, StrId::STR_MONTH_12};
  return ids[std::clamp(month, 1, 12) - 1];
}

StrId weekdayStrId(const int weekday) {
  static const StrId ids[] = {StrId::STR_STATS_SUN, StrId::STR_STATS_MON, StrId::STR_STATS_TUE, StrId::STR_STATS_WED,
                              StrId::STR_STATS_THU, StrId::STR_STATS_FRI, StrId::STR_STATS_SAT};
  return ids[std::clamp(weekday, 0, 6)];
}
}  // namespace

void draw(const GfxRenderer& renderer, const Rect rect, const int year, const int month, const bool todayKnown,
         const int todayYear, const int todayMonth, const int todayDay) {
  const int left = rect.x;
  const int gridWidth = rect.width;
  const int top = rect.y;
  const int bottom = rect.y + rect.height;

  // Pinstriped title banner for the month/year, the same knocked-out-label-
  // on-pinstripes treatment System6 uses for its own window/menu title bars,
  // instead of the month just being a line of centered text.
  const int bannerTop = top + 12;
  const int bannerHeight = 32;
  // Single-bordered: this banner already sits inside the screen's own
  // double-line window frame, and a second double border a few pixels in
  // stacked four near-parallel lines across the top of the grid.
  renderer.drawRect(left - 4, bannerTop, gridWidth + 8, bannerHeight);
  for (int dy = 6; dy < bannerHeight - 5; dy += 4) {
    renderer.drawLine(left, bannerTop + dy, left + gridWidth - 1, bannerTop + dy);
  }
  char monthYear[32];
  std::snprintf(monthYear, sizeof(monthYear), "%s %d", I18n::getInstance().get(monthNameStrId(month)), year);
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, monthYear);
  const int titleX = left + (gridWidth - titleWidth) / 2;
  renderer.fillRect(titleX - 8, bannerTop + 3, titleWidth + 16, bannerHeight - 6, false);
  renderer.drawText(UI_12_FONT_ID, titleX, bannerTop + (bannerHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2,
                    monthYear, true, EpdFontFamily::BOLD);

  const int gridTop = bannerTop + bannerHeight + 18;
  const int colWidth = gridWidth / 7;
  const int firstWeekday = dayOfWeek(year, month, 1);
  const int numDays = daysInMonth(year, month);
  const int numRows = (firstWeekday + numDays + 6) / 7;
  const int rowHeight = std::max(24, (bottom - gridTop) / (numRows + 1));

  const int headerFont = UI_10_FONT_ID;
  for (int col = 0; col < 7; ++col) {
    const char* label = I18n::getInstance().get(weekdayStrId(col));
    const int labelWidth = renderer.getTextWidth(headerFont, label);
    const int textX = left + col * colWidth + (colWidth - labelWidth) / 2;
    renderer.drawText(headerFont, textX, gridTop, label, true, EpdFontFamily::BOLD);
  }
  renderer.drawLine(left, gridTop + rowHeight - 6, left + gridWidth, gridTop + rowHeight - 6, 2, true);

  int day = 1;
  for (int row = 0; row < numRows && day <= numDays; ++row) {
    // A dotted rule under each week, like ruled paper on a real desk
    // calendar pad, instead of the weeks just floating in blank space.
    const int ruleY = gridTop + (row + 1) * rowHeight - 6;
    for (int x = left; x < left + gridWidth; x += 6) renderer.drawLine(x, ruleY, x + 2, ruleY);
    for (int col = 0; col < 7; ++col) {
      const int cellIndex = row * 7 + col;
      if (cellIndex < firstWeekday || day > numDays) continue;
      const int cellX = left + col * colWidth;
      const int cellY = gridTop + (row + 1) * rowHeight;
      const bool isToday = todayKnown && year == todayYear && month == todayMonth && day == todayDay;
      char dayText[4];
      std::snprintf(dayText, sizeof(dayText), "%d", day);
      const int textWidth = renderer.getTextWidth(headerFont, dayText);
      const int textX = cellX + (colWidth - textWidth) / 2;
      if (isToday) {
        // A double-bordered badge rather than a flat fill, so today reads
        // like a pinned marker, not just an inverted rectangle.
        const Rect badge{cellX + 2, cellY - 4, colWidth - 4, rowHeight - 6};
        renderer.fillRect(badge.x, badge.y, badge.width, badge.height, true);
        renderer.drawRect(badge.x + 2, badge.y + 2, badge.width - 4, badge.height - 4, false);
        renderer.drawText(headerFont, textX, cellY, dayText, false, EpdFontFamily::BOLD);
      } else {
        renderer.drawText(headerFont, textX, cellY, dayText);
      }
      ++day;
    }
  }
}

}  // namespace CalendarView
