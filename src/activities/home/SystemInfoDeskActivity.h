#pragma once

#include "activities/Activity.h"

// "About This RetroInk" -- a static vanity/diagnostic panel: firmware
// version, storage, and battery.
class SystemInfoDeskActivity final : public Activity {
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;
  // Storage.usedBytes() can walk the whole FAT free-cluster table on first
  // call after boot/idle -- taking real wall-clock seconds. NotStarted draws
  // a "Calculating..." placeholder and returns; only on the NEXT loop() tick
  // (after Back has had a chance to fire first) does Awaiting actually run
  // the blocking call. Splitting it across two ticks like this -- rather
  // than doing the blocking call mid-render -- is what keeps Back responsive
  // instead of leaving the screen stuck while it's calculating.
  enum class StorageState : uint8_t { NotStarted, Awaiting, Ready };
  StorageState storageState_ = StorageState::NotStarted;
  char storageText_[48] = {};

 public:
  SystemInfoDeskActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("SystemInfoDesk", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
