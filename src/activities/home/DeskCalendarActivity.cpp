#include "DeskCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <memory>
#include <cstdio>

#include "DeskDatePickerActivity.h"
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
  requestUpdate();
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
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_DESK_CALENDAR), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_DESK_CALENDAR), nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;
  // No frame drawn here: System6Theme::drawHeader already draws the window
  // body all the way down to the button hints, grow box and all. Drawing
  // another one on top stacked a second window inside the first -- most
  // visible where its corner cut across the grow box.
  const int left = frameX + 16;
  const int gridWidth = frameW - 32;

  CalendarView::draw(renderer, Rect{left, top, gridWidth, bottom - top}, viewYear_, viewMonth_, todayKnown_,
                     todayYear_, todayMonth_, todayDay_);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), picksDate_ ? tr(STR_DESK_SET_DATE) : "",
                                            tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
