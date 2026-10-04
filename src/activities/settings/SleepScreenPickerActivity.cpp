#include "SleepScreenPickerActivity.h"

#include <HalClock.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/boot_sleep/SleepActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct ModeEntry {
  uint8_t mode;
  StrId name;
};

// Left to right: the screens that were there before RetroInk, then RetroInk's
// own, then the desk accessories. The Settings list uses the same order.
constexpr ModeEntry kModes[] = {
    {CrossPointSettings::COVER, StrId::STR_COVER},
    {CrossPointSettings::CUSTOM, StrId::STR_CUSTOM},
    {CrossPointSettings::OVERLAY, StrId::STR_PAGE_OVERLAY},
    {CrossPointSettings::QUICK_RESUME, StrId::STR_QUICK_RESUME},
    {CrossPointSettings::DARK, StrId::STR_RETROINK_REST},
    {CrossPointSettings::RETROINK_SYSTEM_NAP_SLEEP, StrId::STR_SLEEP_SYSTEM_NAP},
    {CrossPointSettings::RETROINK_ERROR_404_SLEEP, StrId::STR_SLEEP_ERROR_404},
    {CrossPointSettings::RETROINK_INSERT_BOOKMARK_SLEEP, StrId::STR_SLEEP_INSERT_BOOKMARK},
    {CrossPointSettings::READING_STATS_SLEEP, StrId::STR_TODAY},
    {CrossPointSettings::MINIMAL_STATS_SLEEP, StrId::STR_BOOK_STATUS},
    {CrossPointSettings::BOOK_WEEK_STATS_SLEEP, StrId::STR_BOOK_WEEK_STATS},
    {CrossPointSettings::DASHBOARD_SLEEP, StrId::STR_READING_YEAR},
    {CrossPointSettings::MOON_PHASE_SLEEP, StrId::STR_MOON_PHASE},
    {CrossPointSettings::EARTH_PHASE_SLEEP, StrId::STR_EARTH_PHASE},
    {CrossPointSettings::DESK_CALENDAR_SLEEP, StrId::STR_DESK_CALENDAR},
    {CrossPointSettings::DESK_DAY_SLEEP, StrId::STR_CALENDAR_DAY_SLEEP},
};
constexpr int kModeCount = sizeof(kModes) / sizeof(kModes[0]);
constexpr int kTitleBarHeight = 36;
constexpr int kDefaultIndexMode = CrossPointSettings::DARK;

const char* nameFor(const uint8_t mode) {
  for (const auto& entry : kModes) {
    if (entry.mode == mode) return I18N.get(entry.name);
  }
  return "";
}
}  // namespace

SleepScreenPickerActivity::Layout SleepScreenPickerActivity::layout() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  Layout l{};
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  l.top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  l.bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  // Under the window: a position line, the dots, and a caption.
  constexpr int kBelow = 96;
  const int windowMaxH = l.bottom - l.top - kBelow;
  // The preview keeps the screen's proportions and is at most 60% of its size.
  int thumbH = std::min(windowMaxH - kTitleBarHeight - 24, pageHeight * 6 / 10);
  int thumbW = thumbH * pageWidth / pageHeight;
  l.winW = thumbW + 40;
  l.winH = thumbH + kTitleBarHeight + 24;
  l.winX = (pageWidth - l.winW) / 2;
  l.winY = l.top + 4;
  l.thumbX = l.winX + (l.winW - thumbW) / 2;
  l.thumbY = l.winY + kTitleBarHeight + 12;
  return l;
}

void SleepScreenPickerActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const bool hasClock = halClock.isAvailable();
  count_ = 0;
  for (const auto& entry : kModes) {
    // Screens built on the real time are left out on a reader with no clock.
    if (!hasClock && sleepScreenNeedsClock(entry.mode)) continue;
    modes_[count_++] = entry.mode;
  }

  index_ = 0;
  for (int i = 0; i < count_; ++i) {
    if (modes_[i] == SETTINGS.sleepScreen) index_ = i;
  }
  // A saved screen that is not in the list (an older one) starts on the default.
  bool found = false;
  for (int i = 0; i < count_; ++i) found = found || modes_[i] == SETTINGS.sleepScreen;
  if (!found) {
    for (int i = 0; i < count_; ++i) {
      if (modes_[i] == kDefaultIndexMode) index_ = i;
    }
  }

  const Layout l = layout();
  thumbW_ = l.winW - 40;
  thumbH_ = l.winH - kTitleBarHeight - 24;
  thumb_ = makeUniqueNoThrow<uint8_t[]>(static_cast<size_t>((thumbW_ + 7) / 8) * thumbH_);
  startPreview();
}

void SleepScreenPickerActivity::onExit() {
  thumb_.reset();
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void SleepScreenPickerActivity::startPreview() {
  const uint8_t mode = modes_[index_];
  drawingShown_ = false;
  if (!SleepActivity::modeHasPreview(mode)) {
    preview_ = Preview::Placeholder;
  } else if (!thumb_) {
    preview_ = Preview::Unavailable;
  } else {
    preview_ = Preview::Drawing;
  }
  requestUpdate();
}

void SleepScreenPickerActivity::step(const int delta) {
  const int next = std::clamp(index_ + delta, 0, count_ - 1);
  if (next == index_) return;
  index_ = next;
  startPreview();
}

void SleepScreenPickerActivity::apply() {
  SETTINGS.sleepScreen = modes_[index_];
  ActivityResult result;
  setResult(std::move(result));
  finish();
}

// Runs once the "Drawing..." frame is on screen: the real sleep screen is drawn
// and shrunk into thumb_ (slow for the Moon and Earth), then shown.
void SleepScreenPickerActivity::computePreview() {
  bool ok = false;
  {
    RenderLock lock(*this);
    SleepActivity sleep(renderer, mappedInput, false);
    ok = sleep.renderPreview(modes_[index_], thumb_.get(), thumbW_, thumbH_);
  }
  preview_ = ok ? Preview::Ready : Preview::Unavailable;
  requestUpdate();
}

void SleepScreenPickerActivity::loop() {
  using B = MappedInputManager::Button;
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) || mappedInput.wasReleased(B::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    apply();
    return;
  }
  if (mappedInput.wasReleased(B::Left)) {
    step(-1);
  } else if (mappedInput.wasReleased(B::Right)) {
    step(1);
  } else {
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Left) step(1);
    if (swipe == MappedInputManager::SwipeDir::Right) step(-1);
    int tapX = 0, tapY = 0;
    if (mappedInput.wasScreenTapped(tapX, tapY)) {
      const Layout l = layout();
      if (tapX >= l.winX && tapX < l.winX + l.winW && tapY >= l.winY && tapY < l.winY + l.winH) {
        apply();
        return;
      }
      step(tapX < renderer.getScreenWidth() / 2 ? -1 : 1);
    }
  }

  if (preview_ == Preview::Drawing && drawingShown_) computePreview();
}

void SleepScreenPickerActivity::render(RenderLock&&) {
  const auto pageWidth = renderer.getScreenWidth();
  const uint8_t mode = modes_[index_];

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_SLEEP_SCREEN), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_SLEEP_SCREEN), nullptr, false);
  }

  const Layout l = layout();
  // The preview window: drop shadow, double frame, and a striped title bar
  // with the screen's name, like a Macintosh window.
  renderer.fillRect(l.winX + 5, l.winY + 5, l.winW, l.winH, true);
  renderer.fillRect(l.winX, l.winY, l.winW, l.winH, false);
  renderer.drawRect(l.winX, l.winY, l.winW, l.winH);
  renderer.drawRect(l.winX + 3, l.winY + 3, l.winW - 6, l.winH - 6);
  for (int y = l.winY + 8; y < l.winY + kTitleBarHeight - 6; y += 4) {
    renderer.drawLine(l.winX + 8, y, l.winX + l.winW - 9, y);
  }
  const char* name = nameFor(mode);
  const int nameWidth = renderer.getTextWidth(UI_12_FONT_ID, name, EpdFontFamily::BOLD);
  const int nameX = l.winX + (l.winW - nameWidth) / 2;
  renderer.fillRect(nameX - 10, l.winY + 4, nameWidth + 20, kTitleBarHeight - 8, false);
  renderer.drawText(UI_12_FONT_ID, nameX, l.winY + (kTitleBarHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, name,
                    true, EpdFontFamily::BOLD);
  renderer.drawLine(l.winX + 4, l.winY + kTitleBarHeight, l.winX + l.winW - 5, l.winY + kTitleBarHeight);

  // The preview itself, or a line saying why there is not one.
  if (preview_ == Preview::Ready && thumb_) {
    const int stride = (thumbW_ + 7) / 8;
    for (int y = 0; y < thumbH_; ++y) {
      const uint8_t* row = thumb_.get() + static_cast<size_t>(y) * stride;
      for (int x = 0; x < thumbW_; ++x) {
        if (row[x / 8] & (0x80 >> (x % 8))) renderer.drawPixel(l.thumbX + x, l.thumbY + y, true);
      }
    }
    renderer.drawRect(l.thumbX - 1, l.thumbY - 1, thumbW_ + 2, thumbH_ + 2);
  } else {
    renderer.drawRect(l.thumbX - 1, l.thumbY - 1, thumbW_ + 2, thumbH_ + 2);
    const char* message = tr(STR_SLEEP_PREVIEW_NONE);
    if (preview_ == Preview::Drawing) message = tr(STR_SLEEP_PREVIEW_DRAWING);
    else if (mode == CrossPointSettings::QUICK_RESUME) message = tr(STR_SLEEP_PREVIEW_QUICK_RESUME);
    else if (mode == CrossPointSettings::OVERLAY) message = tr(STR_SLEEP_PREVIEW_OVERLAY);
    const int font = UI_10_FONT_ID;
    const auto lines = renderer.wrappedText(font, message, thumbW_ - 24, 5);
    const int lineHeight = renderer.getLineHeight(font) + 4;
    int y = l.thumbY + (thumbH_ - static_cast<int>(lines.size()) * lineHeight) / 2;
    for (const auto& line : lines) {
      renderer.drawCenteredText(font, y, line.c_str());
      y += lineHeight;
    }
  }

  // Arrows at the edges show which way there are more screens.
  const int arrowY = l.winY + l.winH / 2;
  if (index_ > 0) {
    for (int i = 0; i < 18; ++i) renderer.drawLine(l.winX - 26 + i / 2, arrowY - i, l.winX - 26 + i / 2, arrowY + i);
  }
  if (index_ < count_ - 1) {
    for (int i = 0; i < 18; ++i) {
      renderer.drawLine(l.winX + l.winW + 26 - i / 2, arrowY - i, l.winX + l.winW + 26 - i / 2, arrowY + i);
    }
  }

  // Position, one dot per screen, and whether this is the one in use.
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  int y = l.winY + l.winH + 18;
  char position[24];
  std::snprintf(position, sizeof(position), tr(STR_SLEEP_POSITION), index_ + 1, count_);
  renderer.drawCenteredText(UI_12_FONT_ID, y, position, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 8;
  const int dotGap = std::min(22, (pageWidth - 40) / std::max(1, count_));
  const int dotsLeft = pageWidth / 2 - (count_ - 1) * dotGap / 2;
  for (int i = 0; i < count_; ++i) {
    const int cx = dotsLeft + i * dotGap;
    if (i == index_) {
      renderer.fillRect(cx - 5, y, 11, 11, true);
    } else {
      renderer.drawRect(cx - 3, y + 2, 7, 7);
    }
  }
  y += 11 + 12;
  renderer.drawCenteredText(UI_10_FONT_ID, y,
                            mode == SETTINGS.sleepScreen ? tr(STR_SLEEP_IN_USE) : tr(STR_SLEEP_SELECT_TO_USE));
  (void)lineHeight;

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();

  // Tell loop() the "Drawing..." frame has been pushed, so the slow part can start.
  if (preview_ == Preview::Drawing) drawingShown_ = true;
}
