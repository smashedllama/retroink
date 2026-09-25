#include "EarthPhaseDeskActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include "CrossPointSettings.h"
#include "components/ClockFormat.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// How long one cached globe stays good for. Ten minutes moves the terminator
// by 2.5 degrees, a visible few pixels at this disc size, which is about the
// slowest cadence that still reads as "moving" without the rebuild's brief
// blank becoming a nuisance.
constexpr uint32_t kSlotMinutes = 10;

// Module-level rather than per-instance, so leaving and re-entering the screen
// inside one slot costs nothing.
std::unique_ptr<EarthPhase::DiscTexture>& cachedTexture() {
  static std::unique_ptr<EarthPhase::DiscTexture> instance;
  return instance;
}
int& cachedRadius() {
  static int radius = -1;
  return radius;
}
uint32_t& cachedSlot() {
  static uint32_t slot = 0xFFFFFFFFu;
  return slot;
}

const EarthPhase::DiscTexture& getOrBuild(const int radius, const uint32_t slot, const float centerLon,
                                          const float declination, const float subsolarLon) {
  auto& texture = cachedTexture();
  if (!texture || cachedRadius() != radius || cachedSlot() != slot) {
    texture = std::make_unique<EarthPhase::DiscTexture>(radius, centerLon, declination, subsolarLon);
    cachedRadius() = radius;
    cachedSlot() = slot;
  }
  return *texture;
}
}  // namespace

uint32_t EarthPhaseDeskActivity::currentSlot() {
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0;
  if (!halClock.isAvailable() || !halClock.getDateTime(year, month, day, hour, minute)) return 0;
  const uint32_t minutes = ((static_cast<uint32_t>(year) * 366 + month * 31 + day) * 24 + hour) * 60 + minute;
  return minutes / kSlotMinutes;
}

bool EarthPhaseDeskActivity::readClock() {
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0;
  if (!halClock.isAvailable() || !halClock.getDateTime(year, month, day, hour, minute)) return false;
  EarthPhase::subsolar(year, month, day, hour, minute, declination_, subsolarLon_);
  centerLon_ = EarthPhase::centerLonForUtcOffsetQ(SETTINGS.clockUtcOffsetQ);
  return formatLocalDate(dateText_, sizeof(dateText_));
}

void EarthPhaseDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  texture_ = nullptr;
  buildState_ = BuildState::NotStarted;
  dateText_[0] = '\0';
  dateAvailable_ = readClock();
  renderedSlot_ = currentSlot();

  // A cache hit is instant, so skip the placeholder entirely and show the real
  // globe straight away -- the placeholder only earns its keep before a build.
  if (dateAvailable_) {
    int diameter, cx, cy;
    computeDiscGeometry(diameter, cx, cy);
    const int radius = diameter / 2;
    auto& cached = cachedTexture();
    if (cached && cachedRadius() == radius && cachedSlot() == renderedSlot_) {
      texture_ = cached.get();
      buildState_ = BuildState::Ready;
    }
  }
  requestUpdate();
}

void EarthPhaseDeskActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void EarthPhaseDeskActivity::computeDiscGeometry(int& diameter, int& cx, int& cy) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;

  const int captionLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int captionY = bottom - captionLineHeight - 12;

  diameter = std::min(frameW - 20, (captionY - top) - 16);
  cx = pageWidth / 2;
  cy = top + (captionY - top) / 2;
}

void EarthPhaseDeskActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (buildState_ == BuildState::Awaiting) {
    // The placeholder is already on the panel; Back above is what keeps this
    // from running at all if the user backed out first.
    int diameter, cx, cy;
    computeDiscGeometry(diameter, cx, cy);
    texture_ = &getOrBuild(diameter / 2, renderedSlot_, centerLon_, declination_, subsolarLon_);
    buildState_ = BuildState::Ready;
    requestUpdate();
    return;
  }

  if (buildState_ == BuildState::Ready && dateAvailable_ && currentSlot() != renderedSlot_) {
    renderedSlot_ = currentSlot();
    readClock();
    buildState_ = BuildState::NotStarted;
    texture_ = nullptr;
    requestUpdate();
  }
}

void EarthPhaseDeskActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_EARTH_PHASE), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_EARTH_PHASE), nullptr, false);
  }

  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;
  // No frame drawn here: System6Theme::drawHeader already draws the window
  // body all the way down to the button hints, grow box and all. Drawing
  // another one on top stacked a second window inside the first -- most
  // visible where its corner cut across the grow box.
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");

  if (!dateAvailable_) {
    renderer.drawCenteredText(UI_10_FONT_ID, top + (bottom - top) / 2, dateUnavailableMessage());
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int captionLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int captionY = bottom - captionLineHeight - 12;
  renderer.drawCenteredText(UI_10_FONT_ID, captionY, dateText_);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  int diameter, cx, cy;
  computeDiscGeometry(diameter, cx, cy);

  if (buildState_ != BuildState::Ready) {
    renderer.drawCenteredText(UI_10_FONT_ID, top + (captionY - top) / 2, tr(STR_SYSINFO_CALCULATING));
    renderer.displayBuffer();
    if (buildState_ == BuildState::NotStarted) buildState_ = BuildState::Awaiting;
    return;
  }

  EarthPhase::draw(renderer, cx, cy, *texture_);
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
