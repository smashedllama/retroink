#include "DeskDayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "components/DeskDate.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int kMaxTitleLines = 4;
constexpr int kRowPadding = 7;

StrId weekdayStrId(const int weekdayMonday0) {
  static const StrId ids[] = {StrId::STR_STATS_MON, StrId::STR_STATS_TUE, StrId::STR_STATS_WED, StrId::STR_STATS_THU,
                              StrId::STR_STATS_FRI, StrId::STR_STATS_SAT, StrId::STR_STATS_SUN};
  return ids[std::clamp(weekdayMonday0, 0, 6)];
}

// "45m", "2h", "1h 30m". Empty when the event has no known end.
std::string durationText(const ics::Occurrence& o) {
  if (o.allDay() || o.endMinute == ics::kNoTime || o.endMinute <= o.startMinute) return {};
  const int minutes = o.endMinute - o.startMinute;
  char buf[16];
  if (minutes < 60) {
    std::snprintf(buf, sizeof(buf), "%dm", minutes);
  } else if (minutes % 60 == 0) {
    std::snprintf(buf, sizeof(buf), "%dh", minutes / 60);
  } else {
    std::snprintf(buf, sizeof(buf), "%dh %dm", minutes / 60, minutes % 60);
  }
  return buf;
}
}  // namespace

void DeskDayActivity::loadDay() {
  events_.eventsOn(*day_, indices_);
  first_ = 0;
}

void DeskDayActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  loadDay();
  requestUpdate();
}

void DeskDayActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void DeskDayActivity::changeDay(const int delta) {
  const int32_t next = *day_ + delta;
  int y, m, d;
  ics::civilFromDays(next, y, m, d);
  if (y < 1970 || y > 2100) return;
  *day_ = next;
  loadDay();
  requestUpdate();
}

void DeskDayActivity::scroll(const int delta) {
  const int next = std::clamp(first_ + delta, 0, maxFirst_);
  if (next == first_) return;
  first_ = next;
  requestUpdate();
}

void DeskDayActivity::loop() {
  using B = MappedInputManager::Button;
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) || mappedInput.wasReleased(B::Back) ||
      mappedInput.wasReleased(B::Confirm)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(B::Left)) {
    changeDay(-1);
    return;
  }
  if (mappedInput.wasReleased(B::Right)) {
    changeDay(1);
    return;
  }
  if (mappedInput.wasReleased(B::Up) || mappedInput.wasReleased(B::PageBack)) {
    scroll(-1);
    return;
  }
  if (mappedInput.wasReleased(B::Down) || mappedInput.wasReleased(B::PageForward)) {
    scroll(1);
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) scroll(std::max(1, visible_));
  if (swipe == MappedInputManager::SwipeDir::Down) scroll(-std::max(1, visible_));
  if (swipe == MappedInputManager::SwipeDir::Left) changeDay(1);
  if (swipe == MappedInputManager::SwipeDir::Right) changeDay(-1);
}

void DeskDayActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int font = UI_10_FONT_ID;
  const int lineHeight = renderer.getLineHeight(font);

  renderer.clearScreen();

  int year, month, dom;
  ics::civilFromDays(*day_, year, month, dom);
  DeskDateTime stamp;
  stamp.year = static_cast<uint16_t>(year);
  stamp.month = static_cast<uint8_t>(month);
  stamp.day = static_cast<uint8_t>(dom);
  char date[40];
  DeskDate::formatDate(stamp, date, sizeof(date));
  char title[64];
  std::snprintf(title, sizeof(title), "%s %s", I18n::getInstance().get(weekdayStrId(ics::weekdayFromDays(*day_))),
                date);

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, title, false);
  } else {
    GUI.drawHeader(renderer, headerRect, title, nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing + 8;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int left = metrics.contentSidePadding + 16;
  const int width = pageWidth - 2 * metrics.contentSidePadding - 32;

  const int total = static_cast<int>(indices_.size());
  if (total == 0) {
    visible_ = 0;
    maxFirst_ = 0;
    renderer.drawCenteredText(font, top + (bottom - top) / 2 - lineHeight / 2, tr(STR_CALENDAR_NO_EVENTS));
  } else {
    // The time column is as wide as the widest label, so titles line up.
    int timeWidth = renderer.getTextWidth(font, tr(STR_CALENDAR_ALL_DAY), EpdFontFamily::BOLD);
    char label[24];
    std::vector<std::string> labels(total);
    std::vector<std::string> durations(total);
    for (int i = 0; i < total; ++i) {
      const ics::Occurrence& o = events_.occurrences[indices_[i]];
      if (o.allDay()) {
        std::snprintf(label, sizeof(label), "%s", tr(STR_CALENDAR_ALL_DAY));
      } else {
        DeskDateTime t;
        t.hour = static_cast<uint8_t>(o.startMinute / 60);
        t.minute = static_cast<uint8_t>(o.startMinute % 60);
        DeskDate::formatTime(t, label, sizeof(label));
      }
      labels[i] = label;
      durations[i] = durationText(o);
      timeWidth = std::max(timeWidth, renderer.getTextWidth(font, durations[i].c_str()));
      timeWidth = std::max(timeWidth, renderer.getTextWidth(font, label, EpdFontFamily::BOLD));
    }
    const int titleX = left + timeWidth + 14;
    const int titleWidth = std::max(60, left + width - titleX);

    std::vector<std::vector<std::string>> lines(total);
    std::vector<int> heights(total);
    for (int i = 0; i < total; ++i) {
      lines[i] = renderer.wrappedText(font, events_.title(events_.occurrences[indices_[i]]), titleWidth, kMaxTitleLines);
      // The duration sits under the start time, so a one-line title still
      // gets a two-line row.
      const size_t rows = std::max<size_t>(durations[i].empty() ? 1 : 2, lines[i].size());
      heights[i] = static_cast<int>(rows) * lineHeight + kRowPadding * 2;
    }

    // How far the list can scroll: the earliest start from which everything
    // after it still fits. Leave a line for the position marker if needed.
    auto fitCount = [&](const int from, const int available) {
      int used = 0, count = 0;
      for (int i = from; i < total; ++i) {
        if (used + heights[i] > available && count > 0) break;
        used += heights[i];
        ++count;
      }
      return count;
    };
    const int available = bottom - top;
    const bool allFit = fitCount(0, available) == total;
    const int listHeight = allFit ? available : available - (lineHeight + 8);
    maxFirst_ = 0;
    if (!allFit) {
      for (int f = total - 1; f >= 0; --f) {
        if (fitCount(f, listHeight) == total - f) maxFirst_ = f;
        else break;
      }
    }
    first_ = std::clamp(first_, 0, maxFirst_);
    visible_ = fitCount(first_, listHeight);

    int y = top;
    for (int i = first_; i < first_ + visible_; ++i) {
      renderer.drawText(font, left, y + kRowPadding, labels[i].c_str(), true, EpdFontFamily::BOLD);
      if (!durations[i].empty()) {
        renderer.drawText(font, left, y + kRowPadding + lineHeight, durations[i].c_str());
      }
      for (size_t l = 0; l < lines[i].size(); ++l) {
        renderer.drawText(font, titleX, y + kRowPadding + static_cast<int>(l) * lineHeight, lines[i][l].c_str());
      }
      y += heights[i];
      const int ruleY = y - 1;
      for (int x = left; x < left + width; x += 6) renderer.drawLine(x, ruleY, x + 2, ruleY);
    }

    if (!allFit) {
      char position[40];
      std::snprintf(position, sizeof(position), "%d-%d / %d", first_ + 1, first_ + visible_, total);
      const int markerY = bottom - lineHeight - 2;
      renderer.drawText(font, left, markerY, position, true, EpdFontFamily::BOLD);
      const int arrowY = markerY + 4;
      const int downX = left + width - 20;
      const int upX = downX - 24;
      if (first_ + visible_ < total) {
        for (int r = 0; r < 6; ++r) renderer.drawLine(downX + r, arrowY + r, downX + 12 - r, arrowY + r);
      }
      if (first_ > 0) {
        for (int r = 0; r < 6; ++r) renderer.drawLine(upX + r, arrowY + 5 - r, upX + 12 - r, arrowY + 5 - r);
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_CALENDAR_PREV_DAY), tr(STR_CALENDAR_NEXT_DAY));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
