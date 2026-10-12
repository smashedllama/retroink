#pragma once
#include "activities/Activity.h"

class CrashActivity final : public Activity {
  std::string panicMessage;

 public:
  explicit CrashActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Crash", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  // Rows and the header Back box cover everything on a touch screen.
  bool wantsKeyBar() const override { return !mappedInput.hasTouchHardware(); }
};
