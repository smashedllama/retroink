#include "DeskCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <memory>
#include <cstdio>

#include "DeskDatePickerActivity.h"
#include "activities/network/CrossPointWebServerActivity.h"
#include "calendar/CalendarSync.h"
#include "components/CalendarView.h"
#include "components/DeskDate.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void DeskCalendarActivity::showToday() {
  DeskDateTime today;
  todayKnown_ = DeskDate::current(today);
  if (!todayKnown_) return;
  todayYear_ = today.year;
  todayMonth_ = today.month;
  todayDay_ = today.day;
  viewYear_ = today.year;
  viewMonth_ = today.month;
}

void DeskCalendarActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  viewYear_ = 2026;
  viewMonth_ = 1;
  exitPending_ = false;
  picksDate_ = !halClock.isAvailable();
  showToday();
  pickerPending_ = picksDate_ && !todayKnown_;
  hasEvents_ = CalendarSync::loadStored(events_);
  selectedDay_ = 0;
  if (todayKnown_) selectedDay_ = todayDay_;
  requestUpdate();
}

void DeskCalendarActivity::openPicker() {
  DeskDateTime start;
  if (!DeskDate::current(start)) start = DeskDate::fallback();
  startActivityForResult(std::make_unique<DeskDatePickerActivity>(renderer, mappedInput, StrId::STR_DESK_CALENDAR,
                                                                  DeskDatePickerActivity::Fields::DateOnly, start),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             showToday();
                           } else if (!todayKnown_) {
                             exitPending_ = true;
                           }
                           requestUpdate();
                         });
}

void DeskCalendarActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void DeskCalendarActivity::pageMonth(const int delta) {
  viewMonth_ += delta;
  if (viewMonth_ < 1) {
    viewMonth_ = 12;
    viewYear_ -= 1;
  } else if (viewMonth_ > 12) {
    viewMonth_ = 1;
    viewYear_ += 1;
  }
  viewYear_ = std::clamp(viewYear_, 1970, 2100);
  agendaScroll_ = 0;
  scrollMode_ = false;
  // A month change lands on today's day in today's month, otherwise the 1st.
  selectedDay_ = (todayKnown_ && viewYear_ == todayYear_ && viewMonth_ == todayMonth_) ? todayDay_ : 1;
  requestUpdate();
}

void DeskCalendarActivity::moveSelection(const int deltaDays) {
  int32_t day = ics::daysFromCivil(viewYear_, viewMonth_, std::max(selectedDay_, 1)) + deltaDays;
  int y, m, d;
  ics::civilFromDays(day, y, m, d);
  if (y < 1970 || y > 2100) return;
  viewYear_ = y;
  viewMonth_ = m;
  selectedDay_ = d;
  agendaScroll_ = 0;
  requestUpdate();
}

void DeskCalendarActivity::scrollAgenda(const int delta) {
  const int maxScroll = std::max(0, agendaTotal_ - agendaVisible_);
  const int next = std::clamp(agendaScroll_ + delta, 0, maxScroll);
  if (next == agendaScroll_) return;
  agendaScroll_ = next;
  requestUpdate();
}

Rect DeskCalendarActivity::agendaRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect grid = gridRect();
  const int agendaTop = grid.y + grid.height + 4;
  const int agendaBottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing;
  return Rect{grid.x, agendaTop + 8, grid.width, agendaBottom - agendaTop - 8};
}

Rect DeskCalendarActivity::gridRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  // With a synced calendar the grid gives up the bottom quarter to the agenda.
  if (hasEvents_) bottom -= (bottom - top) / 4 + 8;
  const int frameX = metrics.contentSidePadding;
  return Rect{frameX + 16, top, pageWidth - 2 * frameX - 32, bottom - top};
}

void DeskCalendarActivity::openOptions() {
  enum class Choice { Sync, Scroll, SetDate };
  std::vector<std::string> options;
  std::vector<Choice> choices;
  options.emplace_back(tr(STR_SYNC_CALENDAR));
  choices.push_back(Choice::Sync);
  if (agendaOverflows()) {
    options.emplace_back(tr(STR_CALENDAR_SCROLL_EVENTS));
    choices.push_back(Choice::Scroll);
  }
  // Without a clock chip the date is picked by hand, so it lives here too.
  if (picksDate_) {
    options.emplace_back(tr(STR_DESK_SET_DATE));
    choices.push_back(Choice::SetDate);
  }
  optionPopup_.show(tr(STR_CALENDAR_OPTIONS), options, 0, [this, choices](const int index) {
    if (index < 0 || index >= static_cast<int>(choices.size())) return;
    switch (choices[index]) {
      case Choice::Sync:
        // Runs the Wi-Fi flow, downloads the feed, and ends back at Home like
        // Sync to Obsidian does.
        startActivityForResult(
            std::make_unique<CrossPointWebServerActivity>(renderer, mappedInput, NetworkMode::SYNC_CALENDAR),
            [this](const ActivityResult&) { requestUpdate(); });
        break;
      case Choice::Scroll:
        scrollMode_ = true;
        break;
      case Choice::SetDate:
        pickerPending_ = true;
        break;
    }
  });
  requestUpdate();
}

void DeskCalendarActivity::loop() {
  if (optionPopup_.isActive()) {
    optionPopup_.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }
  if (scrollMode_) {
    using B = MappedInputManager::Button;
    if (mappedInput.wasReleased(B::Back) || mappedInput.wasReleased(B::Confirm)) {
      scrollMode_ = false;
      requestUpdate();
    } else if (mappedInput.wasReleased(B::Left) || mappedInput.wasReleased(B::Up) ||
               mappedInput.wasReleased(B::PageBack)) {
      scrollAgenda(-1);
    } else if (mappedInput.wasReleased(B::Right) || mappedInput.wasReleased(B::Down) ||
               mappedInput.wasReleased(B::PageForward)) {
      scrollAgenda(1);
    }
    return;
  }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (hasEvents_ && agendaOverflows()) {
    // Swipe up shows later events, like scrolling any list.
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Up) {
      scrollAgenda(std::max(1, agendaVisible_));
      return;
    }
    if (swipe == MappedInputManager::SwipeDir::Down) {
      scrollAgenda(-std::max(1, agendaVisible_));
      return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) || exitPending_) {
    finish();
    return;
  }
  if (pickerPending_) {
    pickerPending_ = false;
    openPicker();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openOptions();
    return;
  }
  // Front Left/Right step a day; the side page buttons step a month.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    moveSelection(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    moveSelection(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
      mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    pageMonth(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    pageMonth(1);
    return;
  }
  int tapX = 0, tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    const int day = CalendarView::dayAt(renderer, gridRect(), viewYear_, viewMonth_, tapX, tapY);
    if (day != 0) {
      selectedDay_ = day;
      agendaScroll_ = 0;
      requestUpdate();
    }
  }
}

void DeskCalendarActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_DESK_CALENDAR), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_DESK_CALENDAR), nullptr, false);
  }

  const Rect grid = gridRect();
  // No frame drawn here: System6Theme::drawHeader already draws the window
  // body all the way down to the button hints, grow box and all. Drawing
  // another one on top stacked a second window inside the first -- most
  // visible where its corner cut across the grow box.
  CalendarView::Markers markers;
  markers.selectedDay = selectedDay_;
  if (hasEvents_) markers.eventMask = events_.monthMask(viewYear_, viewMonth_);
  CalendarView::draw(renderer, grid, viewYear_, viewMonth_, todayKnown_, todayYear_, todayMonth_, todayDay_, markers);

  agendaTotal_ = 0;
  agendaVisible_ = 0;
  if (hasEvents_) {
    const Rect agenda = agendaRect();
    renderer.drawLine(agenda.x, agenda.y - 8, agenda.x + agenda.width, agenda.y - 8, 2, true);
    std::vector<size_t> indices;
    if (selectedDay_ > 0) events_.eventsOn(ics::daysFromCivil(viewYear_, viewMonth_, selectedDay_), indices);
    const int lineHeight = CalendarView::agendaLineHeight(renderer);
    const int capacity = std::max(1, agenda.height / lineHeight);
    const int total = static_cast<int>(indices.size());
    if (total == 0) {
      renderer.drawText(UI_10_FONT_ID, agenda.x, agenda.y, tr(STR_CALENDAR_NO_EVENTS));
    } else {
      // When the day has more events than fit, the last line becomes a
      // "3-5 / 9" position marker.
      const int visible = total > capacity ? std::max(1, capacity - 1) : total;
      agendaTotal_ = total;
      agendaVisible_ = visible;
      agendaScroll_ = std::clamp(agendaScroll_, 0, std::max(0, total - visible));
      const std::vector<size_t> shown(indices.begin() + agendaScroll_, indices.begin() + agendaScroll_ + visible);
      CalendarView::drawAgenda(renderer, agenda, events_, shown, false);
      if (total > visible) {
        char position[40];
        std::snprintf(position, sizeof(position), "%d-%d / %d", agendaScroll_ + 1, agendaScroll_ + visible, total);
        renderer.drawText(UI_10_FONT_ID, agenda.x, agenda.y + visible * lineHeight, position, true,
                          EpdFontFamily::BOLD);
        // Small triangles at the right edge show which way there is more.
        const int arrowY = agenda.y + visible * lineHeight + 4;
        const int downX = agenda.x + agenda.width - 20;
        const int upX = downX - 24;
        if (agendaScroll_ + visible < total) {
          for (int r = 0; r < 6; ++r) renderer.drawLine(downX + r, arrowY + r, downX + 12 - r, arrowY + r);
        }
        if (agendaScroll_ > 0) {
          for (int r = 0; r < 6; ++r) renderer.drawLine(upX + r, arrowY + 5 - r, upX + 12 - r, arrowY + 5 - r);
        }
      }
    }
  }

  const auto labels = scrollMode_
                          ? mappedInput.mapLabels(tr(STR_DONE), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN))
                          : mappedInput.mapLabels(tr(STR_BACK), tr(STR_CALENDAR_OPTIONS), tr(STR_DIR_LEFT),
                                                  tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (optionPopup_.processRender(renderer, mappedInput)) return;
  renderer.displayBuffer();
}
