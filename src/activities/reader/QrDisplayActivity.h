#pragma once
#include <I18n.h>

#include <string>

#include "activities/Activity.h"

class QrDisplayActivity final : public Activity {
 public:
  explicit QrDisplayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& textPayload)
      : Activity("QrDisplay", renderer, mappedInput), textPayload(textPayload) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // Rows and the header Back box cover everything on a touch screen.
  bool wantsKeyBar() const override { return !mappedInput.hasTouchHardware(); }
  bool isReaderActivity() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return true; }

 private:
  std::string textPayload;
};
