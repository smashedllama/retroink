#include "MoonPhaseDeskActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "components/ClockFormat.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Module-level (not per-Activity-instance) cache: the phase only changes
// meaningfully a handful of times a day, so re-visiting this screen later in
// the same boot -- the realistic usage pattern for a "check the moon"
// accessory -- reuses the already-built texture instead of paying the full
// build cost again. Keyed loosely (same radius, phase within a tight
// tolerance) rather than exactly, since phaseFraction_ is a float recomputed
// from the clock each entry and won't bit-for-bit repeat.
std::unique_ptr<MoonPhase::DiscTexture>& cachedTexture() {
  static std::unique_ptr<MoonPhase::DiscTexture> instance;
  return instance;
}
int& cachedRadius() {
  static int radius = -1;
  return radius;
}
float& cachedFraction() {
  static float fraction = -1.0f;
  return fraction;
}

const MoonPhase::DiscTexture& getOrBuildTexture(const int radius, const float fraction) {
  auto& texture = cachedTexture();
  if (!texture || cachedRadius() != radius || std::fabs(cachedFraction() - fraction) > 0.0005f) {
    texture = std::make_unique<MoonPhase::DiscTexture>(radius, fraction);
    cachedRadius() = radius;
    cachedFraction() = fraction;
  }
  return *texture;
}
}  // namespace

void MoonPhaseDeskActivity::onEnter() {
  Activity::onEnter();
  previousOrientation_ = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  dateAvailable_ = false;
  dateText_[0] = '\0';
  texture_ = nullptr;
  buildState_ = BuildState::NotStarted;
  if (halClock.isAvailable()) {
    uint16_t year;
    uint8_t month, day, hour, minute;
    if (halClock.getDateTime(year, month, day, hour, minute)) {
      phaseFraction_ = MoonPhase::phaseFraction(year, month, day, hour, minute);
      dateAvailable_ = formatLocalDate(dateText_, sizeof(dateText_));
    }
  }

  // A cache hit is instant, so skip the placeholder tick entirely and just
  // show the real thing right away -- the placeholder is only useful when a
  // build is actually about to happen.
  if (dateAvailable_) {
    int diameter, cx, cy;
    computeDiscGeometry(diameter, cx, cy);
    const int radius = diameter / 2;
    auto& cached = cachedTexture();
    if (cached && cachedRadius() == radius && std::fabs(cachedFraction() - phaseFraction_) <= 0.0005f) {
      texture_ = cached.get();
      buildState_ = BuildState::Ready;
    }
  }
  requestUpdate();
}

void MoonPhaseDeskActivity::onExit() {
  renderer.setOrientation(previousOrientation_);
  Activity::onExit();
}

void MoonPhaseDeskActivity::computeDiscGeometry(int& diameter, int& cx, int& cy) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int frameX = metrics.contentSidePadding;
  const int frameW = pageWidth - 2 * frameX;

  const int nameLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int captionLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int captionY = bottom - captionLineHeight - 12;
  const int nameY = captionY - nameLineHeight - 4;

  diameter = std::min(frameW - 20, (nameY - top) - 12);
  cx = pageWidth / 2;
  cy = top + (nameY - top) / 2;
}

void MoonPhaseDeskActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (buildState_ == BuildState::Awaiting) {
    // The "Calculating..." placeholder is already on the panel (render()
    // pushed it before arming this) -- Back above is what keeps this from
    // ever running if the user backs out first.
    int diameter, cx, cy;
    computeDiscGeometry(diameter, cx, cy);
    texture_ = &getOrBuildTexture(diameter / 2, phaseFraction_);
    buildState_ = BuildState::Ready;
    requestUpdate();
  }
}

void MoonPhaseDeskActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // Phase name/illumination/date all live in the footer, not the header --
  // the header already carries the title, clock and battery, and cramming a
  // subtitle in there too left everything truncated ("Moon P...", "Waxing
  // Gl...").
  const Rect headerRect = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, headerRect, tr(STR_MOON_PHASE), false);
  } else {
    GUI.drawHeader(renderer, headerRect, tr(STR_MOON_PHASE), nullptr, false);
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

  int diameter, cx, cy;
  computeDiscGeometry(diameter, cx, cy);

  char caption[48];
  std::snprintf(caption, sizeof(caption), tr(STR_MOON_ILLUMINATED), MoonPhase::illuminationPercent(phaseFraction_));
  std::snprintf(caption + std::strlen(caption), sizeof(caption) - std::strlen(caption), "   %s", dateText_);
  const int nameLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int captionLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int captionY = bottom - captionLineHeight - 12;
  const int nameY = captionY - nameLineHeight - 4;
  renderer.drawCenteredText(UI_12_FONT_ID, nameY, MoonPhase::phaseName(phaseFraction_), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, captionY, caption);

  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (buildState_ != BuildState::Ready) {
    // Placeholder only -- push this now and let loop() build (or fetch from
    // cache) on its next tick, once Back has had a chance to fire first.
    renderer.drawCenteredText(UI_10_FONT_ID, (top + nameY) / 2, tr(STR_SYSINFO_CALCULATING));
    renderer.displayBuffer();
    if (buildState_ == BuildState::NotStarted) buildState_ = BuildState::Awaiting;
    return;
  }

  MoonPhase::draw(renderer, cx, cy, *texture_);
  renderer.displayBuffer();
}
