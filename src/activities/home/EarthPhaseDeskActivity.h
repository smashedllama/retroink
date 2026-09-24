#pragma once

#include "activities/Activity.h"
#include "components/EarthPhase.h"

// A day/night Earth globe desk accessory, centred on the user's timezone.
// Building the disc is real per-pixel work on a chip with no hardware FPU, so
// it follows the Moon Phase screen's two tricks: the built texture is cached
// across visits, and a cache miss renders a "Calculating..." placeholder first
// so the build happens on the tick after Back has had a chance to fire.
//
// Unlike the moon, the terminator moves visibly as the day goes on, so this
// rebuilds periodically -- but on the order of ten minutes, not seconds: a
// rebuild blanks the panel briefly, and doing that often is exactly what made
// the desk clock's old periodic refresh so irritating.
class EarthPhaseDeskActivity final : public Activity {
  bool dateAvailable_ = false;
  char dateText_[40] = {};
  float centerLon_ = 0.0f;
  float declination_ = 0.0f;
  float subsolarLon_ = 0.0f;
  // Which ten-minute slot the on-screen globe was built for, so loop() can
  // tell when it is worth paying for a rebuild.
  uint32_t renderedSlot_ = 0;
  // Non-owning -- points into the module-level cache in the .cpp, which
  // outlives this activity so re-entering the screen is instant.
  const EarthPhase::DiscTexture* texture_ = nullptr;
  enum class BuildState : uint8_t { NotStarted, Awaiting, Ready };
  BuildState buildState_ = BuildState::NotStarted;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  // Shared by render() (to place the disc) and loop() (to know what radius to
  // build at), so the two cannot disagree about where the globe goes.
  void computeDiscGeometry(int& diameter, int& cx, int& cy) const;
  // Reads the clock into centerLon_/declination_/subsolarLon_/dateText_.
  // Returns false when the RTC has never been set.
  bool readClock();
  static uint32_t currentSlot();

 public:
  EarthPhaseDeskActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("EarthPhaseDesk", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
