#include "DeskDatePickerActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "components/TouchHeaderBackButton.h"
#include "components/TouchRegistry.h"
#include "components/UITheme.h"
#include "components/themes/BaseTheme.h"
#include "fontIds.h"

namespace {
// Five-minute steps: a minute at a time is tedious to scroll, and five
// minutes moves Earth's terminator by just over a degree.
constexpr int kMinuteStep = 5;

int wrap(const int value, const int low, const int high) {
  const int span = high - low + 1;
  return low + ((value - low) % span + span) % span;
}
}  // namespace

void DeskDatePickerActivity::onEnter() {
  Activity::onEnter();
  DeskDate::normalize(value_);
  value_.minute = static_cast<uint8_t>(value_.minute / kMinuteStep * kMinuteStep);
  selected_ = 0;
  requestUpdate();
}

void DeskDatePickerActivity::adjust(const int delta) {
  switch (static_cast<Field>(selected_)) {
    case Field::Day:
      value_.day = static_cast<uint8_t>(wrap(value_.day + delta, 1, DeskDate::daysInMonth(value_.year, value_.month)));
      break;
    case Field::Month:
      value_.month = static_cast<uint8_t>(wrap(value_.month + delta, 1, 12));
      break;
    case Field::Year:
      value_.year = static_cast<uint16_t>(
          std::clamp<int>(value_.year + delta, DeskDate::kMinYear, DeskDate::kMaxYear));
      break;
    case Field::Hour:
      value_.hour = static_cast<uint8_t>(wrap(value_.hour + delta, 0, 23));
      break;
    case Field::Minute:
      value_.minute = static_cast<uint8_t>(wrap(value_.minute + delta * kMinuteStep, 0, 60 - kMinuteStep));
      break;
    case Field::Zone:
      value_.zoneQ = static_cast<uint8_t>(std::clamp<int>(value_.zoneQ + delta, 0, DeskDate::kMaxZoneQ));
      break;
  }
  // Changing the month or year can leave the day past the end of the month.
  DeskDate::normalize(value_);
  requestUpdate();
}

DeskDatePickerActivity::RowGeometry DeskDatePickerActivity::rowGeometry() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  RowGeometry g{};
  const int frameX = metrics.contentSidePadding;
  g.rowX = frameX + 14;
  g.rowW = renderer.getScreenWidth() - 2 * frameX - 28;
  g.step = 58;
  g.rowH = g.step - 8;
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  g.firstY = top + 10;
  g.buttonW = 52;
  return g;
}

int DeskDatePickerActivity::rowAt(const int x, const int y, int& buttonDelta) const {
  const RowGeometry g = rowGeometry();
  buttonDelta = 0;
  for (int i = 0; i < fieldCount(); ++i) {
    const int top = g.firstY + i * g.step;
    if (x < g.rowX || x >= g.rowX + g.rowW || y < top || y >= top + g.rowH) continue;
    const int plusX = g.rowX + g.rowW - g.buttonW - 6;
    const int minusX = plusX - g.buttonW - 6;
    if (x >= plusX) buttonDelta = 1;
    else if (x >= minusX) buttonDelta = -1;
    return i;
  }
  return -1;
}

// Touch: tap a row to pick it; press and hold its - or + to change it, with the
// same pause-then-repeat feel as the front buttons.
void DeskDatePickerActivity::handleTouch() {
  int x = 0, y = 0;
  int delta = 0;
  const uint32_t now = millis();
  if (mappedInput.wasScreenTouchDown(x, y)) {
    const int row = rowAt(x, y, delta);
    if (row >= 0) {
      selected_ = row;
      if (delta != 0) {
        adjust(delta);
        repeatDelta_ = delta;
        repeatStartMs_ = now;
        lastRepeatMs_ = now;
      } else {
        requestUpdate();
      }
    }
    return;
  }
  if (repeatDelta_ != 0) {
    if (mappedInput.isScreenTouchHeld(x, y)) {
      if (rowAt(x, y, delta) == selected_ && delta == repeatDelta_ && now - repeatStartMs_ > 450 &&
          now - lastRepeatMs_ > 150) {
        adjust(repeatDelta_);
        lastRepeatMs_ = now;
      }
    } else {
      repeatDelta_ = 0;
    }
  }
}

void DeskDatePickerActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    DeskDate::save(value_);
    finish();
    return;
  }
  if (mappedInput.hasTouchHardware()) handleTouch();
  // Side buttons pick the field; front buttons change it.
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected_ = ButtonNavigator::previousIndex(selected_, fieldCount());
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected_ = ButtonNavigator::nextIndex(selected_, fieldCount());
    requestUpdate();
    return;
  }
  valueNavigator_.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjust(-1); });
  valueNavigator_.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjust(1); });
}

void DeskDatePickerActivity::formatValue(const Field field, char* buf, const size_t len) const {
  switch (field) {
    case Field::Day:
      snprintf(buf, len, "%u", static_cast<unsigned>(value_.day));
      break;
    case Field::Month:
      snprintf(buf, len, "%s", I18n::getInstance().get(static_cast<StrId>(static_cast<int>(StrId::STR_MONTH_1) +
                                                                          value_.month - 1)));
      break;
    case Field::Year:
      snprintf(buf, len, "%u", static_cast<unsigned>(value_.year));
      break;
    case Field::Hour:
      if (SETTINGS.clockFormat == 1) {
        const unsigned h12 = value_.hour % 12 == 0 ? 12 : value_.hour % 12;
        snprintf(buf, len, "%u %s", h12, value_.hour < 12 ? "AM" : "PM");
      } else {
        snprintf(buf, len, "%02u", static_cast<unsigned>(value_.hour));
      }
      break;
    case Field::Minute:
      snprintf(buf, len, "%02u", static_cast<unsigned>(value_.minute));
      break;
    case Field::Zone:
      DeskDate::formatZone(value_.zoneQ, buf, len);
      break;
  }
}

void DeskDatePickerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, I18n::getInstance().get(titleId_), false);
  } else {
    GUI.drawHeader(renderer, headerRect, I18n::getInstance().get(titleId_), nullptr, false);
  }

  // No frame of our own: the System 6 header already draws the window body.
  const RowGeometry g = rowGeometry();
  const bool touch = mappedInput.hasTouchHardware();
  const int labelFont = UI_10_FONT_ID;
  const int valueFont = UI_12_FONT_ID;

  static constexpr StrId kLabels[] = {StrId::STR_DESK_DAY,  StrId::STR_DESK_MONTH,  StrId::STR_DESK_YEAR,
                                      StrId::STR_DESK_HOUR, StrId::STR_DESK_MINUTE, StrId::STR_DESK_TIME_ZONE};
  int y = g.firstY;
  for (int i = 0; i < fieldCount(); ++i) {
    const bool active = i == selected_;
    // Registered so tests (and the touch router) can find each row and its buttons.
    TouchRegistry::getInstance().add(Rect{g.rowX, y, g.rowW, g.rowH}, i * 3, TouchRegistry::Item);
    if (touch) {
      const int plusHitX = g.rowX + g.rowW - g.buttonW - 6;
      TouchRegistry::getInstance().add(Rect{plusHitX - g.buttonW - 6, y, g.buttonW, g.rowH}, i * 3 + 1,
                                       TouchRegistry::Item);
      TouchRegistry::getInstance().add(Rect{plusHitX, y, g.buttonW, g.rowH}, i * 3 + 2, TouchRegistry::Item);
    }
    if (active) {
      renderer.fillRect(g.rowX, y, g.rowW, g.rowH, true);
    } else {
      renderer.drawRect(g.rowX, y, g.rowW, g.rowH);
    }
    const char* label = I18n::getInstance().get(kLabels[i]);
    renderer.drawText(labelFont, g.rowX + 14, y + (g.rowH - renderer.getLineHeight(labelFont)) / 2, label, !active);
    char value[32];
    formatValue(static_cast<Field>(i), value, sizeof(value));
    // On touch, the value sits left of two buttons, - and +, at the row's end.
    const int valueRight = touch ? g.rowX + g.rowW - 2 * g.buttonW - 24 : g.rowX + g.rowW - 14;
    const int valueW = renderer.getTextWidth(valueFont, value, EpdFontFamily::BOLD);
    renderer.drawText(valueFont, valueRight - valueW, y + (g.rowH - renderer.getLineHeight(valueFont)) / 2, value,
                      !active, EpdFontFamily::BOLD);
    if (touch) {
      const int plusX = g.rowX + g.rowW - g.buttonW - 6;
      const int minusX = plusX - g.buttonW - 6;
      const int btnY = y + 5;
      const int btnH = g.rowH - 10;
      for (const int bx : {minusX, plusX}) {
        renderer.fillRect(bx, btnY, g.buttonW, btnH, false);
        renderer.drawRect(bx, btnY, g.buttonW, btnH);
        const int cx = bx + g.buttonW / 2;
        const int cy = btnY + btnH / 2;
        renderer.drawLine(cx - 9, cy, cx + 9, cy);
        renderer.drawLine(cx - 9, cy - 1, cx + 9, cy - 1);
        if (bx == plusX) {
          renderer.drawLine(cx, cy - 9, cx, cy + 9);
          renderer.drawLine(cx - 1, cy - 9, cx - 1, cy + 9);
        }
      }
    }
    y += g.step;
  }

  renderer.drawCenteredText(UI_10_FONT_ID, y + 12, touch ? tr(STR_DESK_PICKER_HINT_TOUCH) : tr(STR_DESK_PICKER_HINT));

  // On touch each field has its own - and + buttons.
  const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_DONE), touch ? "" : "-", touch ? "" : "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
