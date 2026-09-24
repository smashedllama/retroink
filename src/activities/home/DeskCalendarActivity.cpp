#include "DeskCalendarActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "components/CalendarView.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void DeskCalendarActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  todayKnown_ = false;
  viewYear_ = 2026;
  viewMonth_ = 1;
  if (halClock.isAvailable()) {
    uint16_t year;
    uint8_t month, day, hour, minute;
    if (halClock.getDateTime(year, month, day, hour, minute)) {
      todayKnown_ = true;
      todayYear_ = year;
      todayMonth_ = month;
      todayDay_ = day;
      viewYear_ = year;
      viewMonth_ = month;
    }
  }
  requestUpdate();
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
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

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
