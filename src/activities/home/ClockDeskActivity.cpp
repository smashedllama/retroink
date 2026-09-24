#include "ClockDeskActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "CrossPointSettings.h"
#include "components/ClockFormat.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

// The simulator's HalClock comes from a separately vendored package (not
// lib/HalClockSim), which predates getTimeWithSeconds() and always reports
// isAvailable() == false anyway -- so on SIMULATOR this always just reports
// unavailable, matching every other clock-dependent screen there.
bool getTimeWithSecondsCompat(uint8_t& hour, uint8_t& minute, uint8_t& second) {
#ifndef SIMULATOR
  return halClock.getTimeWithSeconds(hour, minute, second);
#else
  (void)hour;
  (void)minute;
  (void)second;
  return false;
#endif
}

void drawHand(const GfxRenderer& renderer, const int cx, const int cy, const double angle, const int length,
             const int width) {
  const int x = cx + static_cast<int>(std::lround(std::sin(angle) * length));
  const int y = cy - static_cast<int>(std::lround(std::cos(angle) * length));
  renderer.drawLine(cx, cy, x, y, width, true);
}

// getTime()/getTimeWithSeconds() return the raw RTC value (UTC); formatTime()
// normally applies the user's UTC offset internally, but this screen needs
// the same raw hour/minute/second for the analog hands, the digital digits,
// and the caption text all at once, so the offset is applied once here and
// reused everywhere instead of each caller re-deriving (and, as the analog
// hands previously did, forgetting to).
void applyUtcOffset(uint8_t& hour, uint8_t& minute, uint8_t& second) {
  const int offsetQuarterHours = static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48;
  int totalSeconds = static_cast<int>(hour) * 3600 + static_cast<int>(minute) * 60 + static_cast<int>(second) +
                    offsetQuarterHours * 15 * 60;
  totalSeconds = ((totalSeconds % 86400) + 86400) % 86400;
  hour = static_cast<uint8_t>(totalSeconds / 3600);
  minute = static_cast<uint8_t>((totalSeconds % 3600) / 60);
  second = static_cast<uint8_t>(totalSeconds % 60);
}

void formatTimeText(char* buf, const size_t bufSize, const uint8_t hour, const uint8_t minute, const uint8_t second,
                    const bool haveSeconds, const bool use12Hour) {
  if (use12Hour) {
    const bool pm = hour >= 12;
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;
    if (haveSeconds) {
      std::snprintf(buf, bufSize, "%d:%02u:%02u %s", hour12, static_cast<unsigned>(minute),
                    static_cast<unsigned>(second), pm ? "PM" : "AM");
    } else {
      std::snprintf(buf, bufSize, "%d:%02u %s", hour12, static_cast<unsigned>(minute), pm ? "PM" : "AM");
    }
  } else if (haveSeconds) {
    std::snprintf(buf, bufSize, "%02u:%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(minute),
                 static_cast<unsigned>(second));
  } else {
    std::snprintf(buf, bufSize, "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(minute));
  }
}

// 3x5 pixel-grid digits, one bit per cell (bit 2 = leftmost column). The same
// glyphs the Focus timer's countdown uses (FocusSessionActivity.cpp's
// drawPixelCountdown) -- a low-res bitmap digit is the 1-bit Mac idiom, where
// the seven-segment shapes this used to draw are a 1970s LED-clock one that
// appears nowhere else in the firmware.
constexpr uint8_t kPixelDigits[10][5] = {
    {7, 5, 5, 5, 7},  // 0
    {2, 6, 2, 2, 7},  // 1
    {7, 1, 7, 4, 7},  // 2
    {7, 1, 7, 1, 7},  // 3
    {5, 5, 7, 1, 1},  // 4
    {7, 4, 7, 1, 7},  // 5
    {7, 4, 7, 5, 7},  // 6
    {7, 1, 1, 1, 1},  // 7
    {7, 5, 7, 5, 7},  // 8
    {7, 5, 7, 1, 7},  // 9
};

// One digit as a 3x5 grid of square blocks of side `pixel`. Each block is
// inset by a pixel so the grid reads as separate dots rather than a solid
// mass, matching the countdown's look.
void drawPixelDigit(const GfxRenderer& renderer, const int x, const int y, const int pixel, const int digit) {
  if (digit < 0 || digit > 9) return;
  for (int row = 0; row < 5; ++row) {
    const uint8_t bits = kPixelDigits[digit][row];
    for (int col = 0; col < 3; ++col) {
      if (bits & (1U << (2 - col)))
        renderer.fillRect(x + col * pixel + 1, y + row * pixel + 1, pixel - 2, pixel - 2, true);
    }
  }
}

}  // namespace

void ClockDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  lastMinute_ = -1;
  lastSecond_ = -1;
  ticksSinceFullRefresh_ = 0;
  // The first paint replaces a completely different screen (the Desk
  // Accessories menu), and pushing that much new content through a FAST
  // partial refresh is what left the torn/ghosted image on entry -- take the
  // one clean full refresh up front instead, so the screen settles once and
  // then stays put.
  forceFullRefresh_ = true;
  requestUpdate();
}

void ClockDeskActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void ClockDeskActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    digitalMode_ = !digitalMode_;
    forceFullRefresh_ = true;
    requestUpdate();
    return;
  }
  // A single dedicated button for Show Seconds, bound to the bottom-right
  // hint slot ("Right", whichever physical front button that's mapped to on
  // this device) -- confirmed reliable on-device, unlike touch taps on the
  // hint boxes. Deliberately not Up/Down too: not every device has a side
  // rocker, and toggling from multiple buttons at once was more confusing
  // than useful.
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    showSeconds_ = !showSeconds_;
    lastSecond_ = -1;
    forceFullRefresh_ = true;
    requestUpdate();
    return;
  }

  if (showSeconds_) {
    uint8_t hour = 0, minute = 0, second = 0;
    if (halClock.isAvailable() && getTimeWithSecondsCompat(hour, minute, second) &&
        static_cast<int8_t>(second) != lastSecond_) {
      lastSecond_ = static_cast<int8_t>(second);
      // Hand the repaint to the render task rather than driving the panel
      // from here: this loop has to keep calling HalGPIO::update() to catch
      // button edges, and it cannot do that while blocked in a refresh.
      // The header carries its own clock and battery readout and sits
      // outside the partial-refresh region, so it freezes at whatever time
      // the last full redraw ran (seen on-device: header reading 10:47 while
      // the caption below read 10:53) -- take the full path on the minute
      // rollover so it keeps up, and the cheap partial path otherwise.
      if (static_cast<int8_t>(minute) != lastMinute_) {
        lastMinute_ = static_cast<int8_t>(minute);
        pendingSecondsTick_ = false;
      } else {
        pendingSecondsTick_ = true;
      }
      requestUpdate();
    }
  } else {
    uint8_t hour = 0, minute = 0;
    if (halClock.isAvailable() && halClock.getTime(hour, minute) && static_cast<int8_t>(minute) != lastMinute_) {
      lastMinute_ = static_cast<int8_t>(minute);
      requestUpdate();
    }
  }
}

void ClockDeskActivity::drawContent(uint8_t hour, uint8_t minute, uint8_t second, const bool haveSeconds) const {
  applyUtcOffset(hour, minute, second);

  const int captionLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int captionY = contentY_ + contentH_ - captionLineHeight;

  if (digitalMode_) {
    // Hours, then minutes, then seconds when enabled, each stacked in its own
    // little System 6 window: drop shadow, double border, and a pinstriped
    // title band with the label knocked out of it -- the same treatment the
    // theme gives its real window title bars. Reads as a stack of windows on
    // the desktop rather than one wide LCD strip.
    const bool twelveHour = SETTINGS.clockFormat == 1;
    const int displayHour = twelveHour ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    const int values[3] = {displayHour, minute, second};
    const StrId labels[3] = {StrId::STR_HOURS, StrId::STR_MINUTES, StrId::STR_SECONDS};

    const int stackLeft = contentX_ + 10;
    // The 3px drop shadow hangs off the right edge, so keep it inside.
    const int stackWidth = contentW_ - 23;
    const int stackTop = contentY_ + 12;
    const int stackBottom = captionY - 18;
    const int rows = haveSeconds ? 3 : 2;
    constexpr int kRowGap = 12;
    const int rowHeight = (stackBottom - stackTop - kRowGap * (rows - 1)) / rows;

    for (int row = 0; row < rows; ++row) {
      const int y = stackTop + row * (rowHeight + kRowGap);
      renderer.fillRect(stackLeft + 3, y + 3, stackWidth, rowHeight, true);
      renderer.fillRect(stackLeft, y, stackWidth, rowHeight, false);
      renderer.drawRect(stackLeft, y, stackWidth, rowHeight);
      renderer.drawRect(stackLeft + 2, y + 2, stackWidth - 4, rowHeight - 4);

      constexpr int kBandHeight = 24;
      const int bandTop = y + 6;
      for (int dy = 4; dy < kBandHeight - 3; dy += 4)
        renderer.drawLine(stackLeft + 8, bandTop + dy, stackLeft + stackWidth - 9, bandTop + dy);
      // tr() is a macro that pastes StrId:: onto its argument, so it cannot take
      // a runtime value -- go through the getter directly for the per-row label.
      const char* label = I18n::getInstance().get(labels[row]);
      const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD);
      const int labelX = stackLeft + (stackWidth - labelWidth) / 2;
      renderer.fillRect(labelX - 8, bandTop, labelWidth + 16, kBandHeight, false);
      renderer.drawText(UI_10_FONT_ID, labelX, bandTop + (kBandHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                        label, true, EpdFontFamily::BOLD);

      // Two digits side by side with a one-cell gap: 3 + 1 + 3 = 7 cells
      // across, 5 down. No ink-centring fudge needed the way the seven-segment
      // version did -- this font's 1 fills its full three columns.
      const int digitsTop = bandTop + kBandHeight + 4;
      const int digitsBottom = y + rowHeight - 10;
      const int pixel = std::max(2, std::min((stackWidth - 36) / 7, (digitsBottom - digitsTop) / 5));
      const int digitsX = stackLeft + (stackWidth - pixel * 7) / 2;
      const int digitsY = digitsTop + (digitsBottom - digitsTop - pixel * 5) / 2;
      drawPixelDigit(renderer, digitsX, digitsY, pixel, values[row] / 10);
      drawPixelDigit(renderer, digitsX + pixel * 4, digitsY, pixel, values[row] % 10);

      if (twelveHour && row == 0) {
        const char* ampm = hour >= 12 ? "PM" : "AM";
        const int ampmWidth = renderer.getTextWidth(UI_10_FONT_ID, ampm);
        renderer.drawText(UI_10_FONT_ID, stackLeft + stackWidth - ampmWidth - 14,
                          y + rowHeight - renderer.getLineHeight(UI_10_FONT_ID) - 9, ampm, true, EpdFontFamily::BOLD);
      }
    }
  } else {
    // A real analog face -- rim, 12 ticks (bold at 12/3/6/9), hour and
    // minute hands.
    const int faceCx = contentX_ + contentW_ / 2;
    const int faceCy = contentY_ + (captionY - contentY_) / 2;
    const int faceRadius = std::min(contentW_ / 2 - 14, (captionY - contentY_) / 2 - 10);

    renderer.drawArc(faceRadius, faceCx, faceCy, -1, -1, 3, true);
    renderer.drawArc(faceRadius, faceCx, faceCy, -1, 1, 3, true);
    renderer.drawArc(faceRadius, faceCx, faceCy, 1, -1, 3, true);
    renderer.drawArc(faceRadius, faceCx, faceCy, 1, 1, 3, true);

    for (int i = 0; i < 12; ++i) {
      const double angle = i / 12.0 * 2.0 * kPi;
      const bool major = i % 3 == 0;
      const int tickLen = major ? 16 : 9;
      const int lineWidth = major ? 3 : 1;
      const int outerR = faceRadius - 4;
      const int innerR = outerR - tickLen;
      const int x1 = faceCx + static_cast<int>(std::lround(std::sin(angle) * outerR));
      const int y1 = faceCy - static_cast<int>(std::lround(std::cos(angle) * outerR));
      const int x2 = faceCx + static_cast<int>(std::lround(std::sin(angle) * innerR));
      const int y2 = faceCy - static_cast<int>(std::lround(std::cos(angle) * innerR));
      renderer.drawLine(x1, y1, x2, y2, lineWidth, true);
    }

    const double minuteAngle = (minute / 60.0) * 2.0 * kPi;
    const double hourAngle = ((hour % 12) + minute / 60.0) / 12.0 * 2.0 * kPi;
    drawHand(renderer, faceCx, faceCy, hourAngle, faceRadius * 5 / 10, 4);
    drawHand(renderer, faceCx, faceCy, minuteAngle, faceRadius * 8 / 10, 2);
    if (haveSeconds) {
      const double secondAngle = (second / 60.0) * 2.0 * kPi;
      drawHand(renderer, faceCx, faceCy, secondAngle, faceRadius * 9 / 10, 1);
    }
    renderer.fillRect(faceCx - 4, faceCy - 4, 8, 8, true);
  }

  char dateText[40] = {};
  const bool haveDate = formatLocalDate(dateText, sizeof(dateText));
  char caption[56];
  if (digitalMode_ && haveDate) {
    // The stacked panels above already read out the time; repeating it here
    // would just be the same thing twice, so the caption carries the date.
    std::snprintf(caption, sizeof(caption), "%s", dateText);
  } else {
    char timeText[12];
    formatTimeText(timeText, sizeof(timeText), hour, minute, second, haveSeconds, SETTINGS.clockFormat == 1);
    std::snprintf(caption, sizeof(caption), "%s%s%s", timeText, haveDate ? "   " : "", haveDate ? dateText : "");
  }
  renderer.fillRect(contentX_, captionY - 2, contentW_, captionLineHeight + 4, false);
  renderer.drawCenteredText(UI_10_FONT_ID, captionY, caption, true,
                            digitalMode_ ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD);
}

void ClockDeskActivity::tickSecondsLocked() {
  uint8_t hour = 0, minute = 0, second = 0;
  if (!halClock.isAvailable() || !getTimeWithSecondsCompat(hour, minute, second)) return;

  // Clear just the content region (inside the outer double-border frame,
  // which stays untouched) and redraw into it, then push a byte-aligned
  // partial refresh -- a full displayBuffer() every second would be both
  // far slower than the tick interval and a lot of visible flashing.
  renderer.fillRect(contentX_, contentY_, contentW_, contentH_, false);
  drawContent(hour, minute, second, true);

  // Every partial refresh here is FAST mode, which can accumulate visible
  // ghosting over a long viewing session -- a periodic full refresh caps
  // that instead of letting it build up indefinitely. Once every 5 minutes,
  // not every 20s: a full refresh blanks the whole panel for ~2s and the
  // second hand skips over it, which reads as a glitch rather than as
  // maintenance when it happens often.
  constexpr uint16_t kTicksBetweenFullRefresh = 300;
  if (++ticksSinceFullRefresh_ >= kTicksBetweenFullRefresh) {
    ticksSinceFullRefresh_ = 0;
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
    return;
  }

  const int alignedX = contentX_ & ~7;
  const int alignedRight = (contentX_ + contentW_ + 8) & ~7;
  renderer.displayWindow(alignedX, contentY_, alignedRight - alignedX, contentH_);
}

void ClockDeskActivity::render(RenderLock&&) {
  // Seconds-only update: repaint the content region and push a partial
  // refresh instead of rebuilding the whole screen. Runs here, on the render
  // task, holding the lock this was handed -- so the main loop stays free to
  // poll buttons while the panel is busy. contentW_ guards the case where no
  // full render has run yet to establish the layout.
  if (pendingSecondsTick_.exchange(false) && !forceFullRefresh_.load() && contentW_ > 0) {
    tickSecondsLocked();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_DESK_CLOCK), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_DESK_CLOCK), nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;
  // No frame drawn here: System6Theme::drawHeader already draws the window
  // body all the way down to the button hints, grow box and all. Drawing
  // another one on top stacked a second window inside the first -- most
  // visible where its corner cut across the grow box.
  contentX_ = frameX + 4;
  contentY_ = top + 4;
  contentW_ = frameW - 8;
  contentH_ = (bottom - top) - 8;

  uint8_t hour = 0, minute = 0, second = 0;
  bool haveTime = false;
  bool haveSeconds = false;
  if (halClock.isAvailable()) {
    if (showSeconds_ && getTimeWithSecondsCompat(hour, minute, second)) {
      haveTime = true;
      haveSeconds = true;
    } else if (halClock.getTime(hour, minute)) {
      haveTime = true;
    }
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), haveTime ? (digitalMode_ ? tr(STR_ANALOG) : tr(STR_DIGITAL)) : "", "",
                            haveTime ? tr(STR_SECONDS) : "");

  if (!haveTime) {
    renderer.drawCenteredText(UI_10_FONT_ID, top + (bottom - top) / 2, tr(STR_SET_DATE_TIME));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  drawContent(hour, minute, second, haveSeconds);

  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  // A full render defaults to FAST_REFRESH; force a clean FULL_REFRESH right
  // after a mode-changing toggle (Analog/Digital or Show Seconds) so that
  // transition doesn't leave the ghosting "tear" partial refreshes can
  // accumulate.
  const bool fullRefresh = forceFullRefresh_.exchange(false);
  renderer.displayBuffer(fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
}
