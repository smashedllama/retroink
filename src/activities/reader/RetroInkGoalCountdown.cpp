#include "RetroInkGoalCountdown.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "ReadingDeskStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

RetroInkGoalCountdown::Sample RetroInkGoalCountdown::sample(const ReadingStatsDateTime& sessionStart,
                                                             const uint32_t sessionSeconds,
                                                             const uint32_t pageSeconds) {
  Sample result;
  if (SETTINGS.uiTheme != CrossPointSettings::SYSTEM6 || !SETTINGS.showReadingGoalCountdown ||
      !SETTINGS.shouldTrackReadingStats()) return result;

  ReadingStatsDateTime now;
  if (!getCurrentLocalReadingStatsDateTime(now)) return result;
  result.day = readingStatsDayIndex(now.date);
  const uint32_t stored = ReadingDeskStore::secondsForDate(now.date);
  const uint32_t elapsed = sessionSeconds > UINT32_MAX - pageSeconds ? UINT32_MAX : sessionSeconds + pageSeconds;
  uint32_t activeToday = 0;
  if (sessionStart.isValid() && compareReadingStatsDate(sessionStart.date, now.date) <= 0) {
    ReadingStatsDateTime cursor = sessionStart;
    uint32_t remaining = elapsed;
    while (remaining) {
      const uint32_t untilMidnight = 86400U - static_cast<uint32_t>(cursor.hour) * 3600U -
                                     static_cast<uint32_t>(cursor.minute) * 60U - cursor.second;
      const uint32_t segment = std::min(remaining, untilMidnight);
      if (compareReadingStatsDate(cursor.date, now.date) == 0) activeToday += segment;
      remaining -= segment;
      addSecondsToReadingStatsDateTime(cursor, segment);
      if (compareReadingStatsDate(cursor.date, now.date) > 0) break;
    }
  }
  const uint32_t goalSeconds = static_cast<uint32_t>(SETTINGS.readingGoalMinutes) * 60U;
  const uint32_t total = stored > UINT32_MAX - activeToday ? UINT32_MAX : stored + activeToday;
  const uint32_t left = total >= goalSeconds ? 0 : goalSeconds - total;
  result.minutesLeft = static_cast<uint16_t>((left + 59U) / 60U);
  result.valid = true;
  return result;
}

void RetroInkGoalCountdown::badgeRect(const GfxRenderer& renderer, int& x, int& y, int& w, int& h) {
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  (void)bottom;
  (void)left;
  // Sized to the widest text the badge can show today, not a fixed width: a
  // fixed 112px clipped "180 min left" (goals run to 180). And not the live
  // text either -- the badge repaints by partial refresh each minute, so a box
  // that shrank from "10 min" to "9 min" would leave the old edges behind.
  // Every digit is measured as an 8, the widest, at the goal's digit count.
  const unsigned goal = SETTINGS.readingGoalMinutes;
  const unsigned widestMinutes = goal >= 100 ? 888 : (goal >= 10 ? 88 : 8);
  char widest[28];
  snprintf(widest, sizeof(widest), tr(STR_READING_GOAL_BADGE), widestMinutes);
  const int textWidth =
      std::max(renderer.getTextWidth(UI_10_FONT_ID, widest, EpdFontFamily::BOLD),
               renderer.getTextWidth(UI_10_FONT_ID, tr(STR_GOAL_REACHED_BADGE), EpdFontFamily::BOLD));
  // Capped well short of the centre, where the reader's clock sits.
  w = std::min(textWidth + 24, renderer.getScreenWidth() * 2 / 5);
  h = 32;
  x = renderer.getScreenWidth() - right - w - 4;
  y = top + UITheme::getInstance().getMetrics().topPadding;
}

void RetroInkGoalCountdown::drawBadge(const GfxRenderer& renderer, const Sample& value, const bool darkMode,
                                      const bool flash) const {
  int x, y, w, h;
  badgeRect(renderer, x, y, w, h);
  const bool black = flash ? !darkMode : darkMode;
  renderer.fillRect(x, y, w, h, black);
  renderer.drawRect(x, y, w, h, !black);
  if (flash) return;
  char text[28];
  if (value.minutesLeft == 0) {
    snprintf(text, sizeof(text), "%s", tr(STR_GOAL_REACHED_BADGE));
  } else {
    snprintf(text, sizeof(text), tr(STR_READING_GOAL_BADGE), static_cast<unsigned>(value.minutesLeft));
  }
  // Measured in bold because it's drawn in bold -- regular weight is narrower,
  // which centred the text too far right and pushed it over the border.
  const int textX = x + (w - renderer.getTextWidth(UI_10_FONT_ID, text, EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_10_FONT_ID, textX, y + (h - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                    text, !black, EpdFontFamily::BOLD);
}

void RetroInkGoalCountdown::drawOnPage(const GfxRenderer& renderer, const Sample& value, const bool darkMode) {
  if (!value.valid) return;
  if (day_ != value.day) {
    day_ = value.day;
    lastMinutesLeft_ = -1;
  }
  drawBadge(renderer, value, darkMode);
  // A page render establishes the baseline; animation is reserved for a live
  // transition from remaining minutes to zero, never for opening a completed day.
  if (lastMinutesLeft_ < 0) lastMinutesLeft_ = static_cast<int16_t>(value.minutesLeft);
}

void RetroInkGoalCountdown::tick(const GfxRenderer& renderer, const Sample& value, const bool darkMode) {
  if (!value.valid) return;
  if (day_ != value.day) {
    day_ = value.day;
    lastMinutesLeft_ = -1;
  }
  if (lastMinutesLeft_ == static_cast<int16_t>(value.minutesLeft)) return;
  const bool celebrate = lastMinutesLeft_ > 0 && value.minutesLeft == 0 && celebratedDay_ != value.day;
  int x, y, w, h;
  badgeRect(renderer, x, y, w, h);
  if (celebrate) {
    celebratedDay_ = value.day;
    // Two tiny differential flashes give a Mac dialog-like completion cue.
    for (int i = 0; i < 2; ++i) {
      drawBadge(renderer, value, darkMode, true);
      renderer.displayWindow(x, y, w, h);
      drawBadge(renderer, value, !darkMode, true);
      renderer.displayWindow(x, y, w, h);
    }
  }
  drawBadge(renderer, value, darkMode);
  renderer.displayWindow(x, y, w, h);
  lastMinutesLeft_ = static_cast<int16_t>(value.minutesLeft);
}
