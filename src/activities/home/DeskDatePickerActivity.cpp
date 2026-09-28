#include "DeskDatePickerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "components/TouchHeaderBackButton.h"
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
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, I18n::getInstance().get(titleId_), false);
  } else {
    GUI.drawHeader(renderer, headerRect, I18n::getInstance().get(titleId_), nullptr, false);
  }

  // No frame of our own: the System 6 header already draws the window body.
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int rowX = frameX + 14;
  const int rowW = pageWidth - 2 * frameX - 28;
  constexpr int kRowHeight = 58;
  const int labelFont = UI_10_FONT_ID;
  const int valueFont = UI_12_FONT_ID;

  static constexpr StrId kLabels[] = {StrId::STR_DESK_DAY,  StrId::STR_DESK_MONTH,  StrId::STR_DESK_YEAR,
                                      StrId::STR_DESK_HOUR, StrId::STR_DESK_MINUTE, StrId::STR_DESK_TIME_ZONE};
  int y = top + 10;
  for (int i = 0; i < fieldCount(); ++i) {
    const bool active = i == selected_;
    const int rowH = kRowHeight - 8;
    if (active) {
      renderer.fillRect(rowX, y, rowW, rowH, true);
    } else {
      renderer.drawRect(rowX, y, rowW, rowH);
    }
    const char* label = I18n::getInstance().get(kLabels[i]);
    renderer.drawText(labelFont, rowX + 14, y + (rowH - renderer.getLineHeight(labelFont)) / 2, label, !active);
    char value[32];
    formatValue(static_cast<Field>(i), value, sizeof(value));
    const int valueW = renderer.getTextWidth(valueFont, value, EpdFontFamily::BOLD);
    renderer.drawText(valueFont, rowX + rowW - 14 - valueW, y + (rowH - renderer.getLineHeight(valueFont)) / 2, value,
                      !active, EpdFontFamily::BOLD);
    y += kRowHeight;
  }

  renderer.drawCenteredText(UI_10_FONT_ID, y + 12, tr(STR_DESK_PICKER_HINT));

  const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), tr(STR_DONE), "-", "+");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
