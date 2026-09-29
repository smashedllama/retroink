#include "DeskCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <memory>
#include <cstdio>

#include "DeskDatePickerActivity.h"
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
  if (hasEvents_ && todayKnown_) selectedDay_ = todayDay_;
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
  // Keep a selection only where it can be seen: today's day in today's month,
  // otherwise the 1st.
  if (hasEvents_) {
    selectedDay_ = (todayKnown_ && viewYear_ == todayYear_ && viewMonth_ == todayMonth_) ? todayDay_ : 1;
  }
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
  requestUpdate();
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

void DeskCalendarActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
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
  if (picksDate_ && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openPicker();
    return;
  }
  if (hasEvents_) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      moveSelection(-1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      moveSelection(1);
      return;
    }
    int tapX = 0, tapY = 0;
    if (mappedInput.wasScreenTapped(tapX, tapY)) {
      const int day = CalendarView::dayAt(renderer, gridRect(), viewYear_, viewMonth_, tapX, tapY);
      if (day != 0) {
        selectedDay_ = day;
        requestUpdate();
        return;
      }
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    pageMonth(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    pageMonth(1);
    return;
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
  if (hasEvents_) {
    markers.eventMask = events_.monthMask(viewYear_, viewMonth_);
    markers.selectedDay = selectedDay_;
  }
  CalendarView::draw(renderer, grid, viewYear_, viewMonth_, todayKnown_, todayYear_, todayMonth_, todayDay_, markers);

  if (hasEvents_) {
    const int agendaTop = grid.y + grid.height + 4;
    const int agendaBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
    renderer.drawLine(grid.x, agendaTop, grid.x + grid.width, agendaTop, 2, true);
    std::vector<size_t> indices;
    if (selectedDay_ > 0) events_.eventsOn(ics::daysFromCivil(viewYear_, viewMonth_, selectedDay_), indices);
    const Rect agenda{grid.x, agendaTop + 8, grid.width, agendaBottom - agendaTop - 8};
    if (indices.empty()) {
      renderer.drawText(UI_10_FONT_ID, agenda.x, agenda.y, tr(STR_CALENDAR_NO_EVENTS));
    } else {
      CalendarView::drawAgenda(renderer, agenda, events_, indices, false);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), picksDate_ ? tr(STR_DESK_SET_DATE) : "",
                                            tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
