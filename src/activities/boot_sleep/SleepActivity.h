#pragma once
#include <string>
#include <utility>

#include "CrossPointSettings.h"
#include "activities/Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool canSnapshotOverlayBackground,
                         std::string currentBookPath = {}, bool fromTimeout = false,
                         GfxRenderer::Orientation sleepPopupOrientation = GfxRenderer::Orientation::Portrait)
      : Activity("Sleep", renderer, mappedInput),
        canSnapshotOverlayBackground(canSnapshotOverlayBackground),
        currentBookPath(std::move(currentBookPath)),
        fromTimeout(fromTimeout),
        sleepPopupOrientation(sleepPopupOrientation) {}
  void onEnter() override;

  // Draws the sleep screen for `mode` exactly as it would be drawn when the
  // reader goes to sleep (your current book, stats and date included), without
  // showing it on the panel, and shrinks it into `thumb`: a 1-bit bitmap,
  // thumbW x thumbH, rows padded to whole bytes, black = 1. Returns false if
  // the mode has no preview (Quick Resume and Page Overlay show what was on
  // screen, which does not exist yet) or the thumbnail is empty.
  bool renderPreview(uint8_t mode, uint8_t* thumb, int thumbW, int thumbH);
  static bool modeHasPreview(uint8_t mode);

 private:
  void renderForMode(uint8_t mode) const;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderReadingStatsSleepScreen() const;
  void renderMinimalSleepScreen() const;
  void renderMinimalStatsSleepScreen() const;
  void renderBookWeekStatsSleepScreen() const;
  void renderDashboardSleepScreen() const;
  void renderMoonPhaseSleepScreen() const;
  void renderDeskCalendarSleepScreen() const;
  void renderDeskDaySleepScreen() const;
  void renderEarthPhaseSleepScreen() const;
  bool renderBitmapSleepScreen(Bitmap& bitmap, bool forceFastNoGreyscale = false) const;
  void renderLastScreenSleepScreen() const;
  void renderBlankSleepScreen() const;
  void renderOverlaySleepScreen() const;
  mutable bool previewMode_ = false;  // draw black and white only, for renderPreview()
  bool canSnapshotOverlayBackground = false;
  bool overlayBackgroundBufferStored = false;
  uint8_t clockVisibilityBeforeSleep = CrossPointSettings::HIDE_CLOCK_ALWAYS;
  std::string currentBookPath;
  bool fromTimeout = false;
  GfxRenderer::Orientation sleepPopupOrientation = GfxRenderer::Orientation::Portrait;
};
