#pragma once

#include <atomic>

#include "activities/Activity.h"

// A clean System6 desk clock: big time + weekday/date, meant to be left open
// on the desk. Redraws once a minute by default (checked each loop() tick
// against the RTC, which HalClock itself only polls every ~10s) rather than
// on any timer of its own. The bottom-right hint button (bound to the
// device's "Right" front button, whichever physical button that's mapped to)
// toggles "Show Seconds" directly -- a long-press-Confirm options overlay was
// tried first and didn't reliably register on device, so this is a plain,
// hard-to-get-wrong single button instead. With seconds on, only the
// digits/caption region repaints once a second via a partial refresh, not
// the whole screen.
class ClockDeskActivity final : public Activity {
  int8_t lastMinute_ = -1;
  int8_t lastSecond_ = -1;
  bool digitalMode_ = false;
  bool showSeconds_ = false;
  // The content region (inside the outer frame, below the header) that
  // render() drew into -- reused by tickSeconds() so its partial refresh
  // repaints exactly the area render() owns, no more. Plain ints rather
  // than a Rect so this header doesn't need BaseTheme.h just for a member.
  int contentX_ = 0, contentY_ = 0, contentW_ = 0, contentH_ = 0;
  // A full displayBuffer() defaults to FAST_REFRESH, which can leave visible
  // ghosting on a big content change (confirmed on-device: a diagonal "tear"
  // appearing right when Show Seconds is toggled, fading over the next few
  // partial-refresh ticks). Set before requestUpdate() on a mode-changing
  // toggle so that one render uses a clean FULL_REFRESH instead; consumed
  // (reset to false) inside render(). Atomic because loop() (main task) sets
  // it while render() (render task) reads and clears it.
  std::atomic<bool> forceFullRefresh_{false};
  // Set by loop() when the only thing that changed is the second, so render()
  // takes the cheap partial-refresh path over the content region instead of
  // repainting the whole screen. Cleared by render() either way.
  std::atomic<bool> pendingSecondsTick_{false};
  // Ticks accumulate their own ghosting over time since they're always
  // partial/fast refreshes, so a periodic full refresh caps that -- but a
  // full refresh blanks the panel for ~2s and skips the second hand while it
  // runs, so this is deliberately infrequent (minutes, not seconds): the
  // blackout is far more annoying to watch than slow ghosting is to look at.
  uint16_t ticksSinceFullRefresh_ = 0;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  // Draws the analog face or digital bezel, plus the caption line, into the
  // content region. Shared by the full redraw and the per-second partial
  // refresh, so the two can never disagree on layout.
  void drawContent(uint8_t hour, uint8_t minute, uint8_t second, bool haveSeconds) const;
  // Repaints just the content region and pushes a partial refresh. MUST be
  // called only from render(), which already holds the RenderLock -- the
  // mutex is not recursive, so taking it again here would deadlock. Driving
  // the panel from loop() instead is what made buttons need two presses:
  // a refresh blocks for ~500ms, HalGPIO::update() is a poll rather than an
  // interrupt, and any press that began and ended inside that gap was never
  // sampled at all.
  void tickSecondsLocked();

 public:
  ClockDeskActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("ClockDesk", renderer, input) {}
  // A desk clock exists to sit there showing the time, so it holds off the
  // inactivity timer the same way the focus timer does. This also keeps
  // main.cpp's idle path from kicking in (it drops the CPU to its low-power
  // clock and stretches the loop delay after 3s without a button press),
  // which is what made the once-a-second redraw stutter while just watching
  // the clock.
  bool preventAutoSleep() override { return true; }
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
