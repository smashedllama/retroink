#pragma once

#include "activities/Activity.h"
#include "components/MoonPhase.h"

// A System-6 moon phase desk accessory, rendered from a real embedded lunar
// photo (see MoonPhase.h). Building the disc is a real, measured cost on a
// chip with no hardware FPU (confirmed on-device: several seconds for a
// near-full-screen disc even after the available per-pixel optimizations --
// this is close to the actual hardware floor for computing it fresh every
// time). Two things make that tolerable without shrinking the disc: the
// built texture is cached across visits within the same boot (see
// MoonPhaseDeskActivity.cpp's anonymous-namespace cache) so this is only
// slow the first time the phase/size changes, not on every open; and
// building it is deferred to the tick after a "Calculating..." placeholder
// is already on the panel, so a cache-miss build doesn't look like a freeze
// and Back still works immediately.
class MoonPhaseDeskActivity final : public Activity {
  float phaseFraction_ = 0.0f;
  bool dateAvailable_ = false;
  char dateText_[40] = {};
  // Non-owning -- points at the module-level cache entry (see the .cpp),
  // which outlives this Activity instance so a later re-visit can reuse it.
  const MoonPhase::DiscTexture* texture_ = nullptr;
  enum class BuildState : uint8_t { NotStarted, Awaiting, Ready };
  BuildState buildState_ = BuildState::NotStarted;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  // Shared by render() (to place the disc and reserve room for it) and
  // loop() (to know what radius to build the texture at) -- duplicating
  // this math between them risks the two disagreeing on where the disc
  // actually goes.
  void computeDiscGeometry(int& diameter, int& cx, int& cy) const;

 public:
  MoonPhaseDeskActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("MoonPhaseDesk", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
