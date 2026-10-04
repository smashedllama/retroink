#pragma once

#include <GfxRenderer.h>

#include <memory>

#include "activities/Activity.h"

// Browse the sleep screens one at a time. Each shows a small preview drawn by
// the real sleep screen code with your actual book, stats and date; Left and
// Right move between them and Select makes the current one the sleep screen.
class SleepScreenPickerActivity final : public Activity {
  static constexpr int kMaxModes = 20;

  enum class Preview : uint8_t {
    Drawing,      // the "Drawing..." frame is up (or about to be); the real preview comes next
    Ready,        // thumb_ holds the preview
    Placeholder,  // a mode that has nothing to draw ahead of time, shown as a description
    Unavailable,  // a preview could not be made
  };

  uint8_t modes_[kMaxModes] = {};
  int count_ = 0;
  int index_ = 0;
  Preview preview_ = Preview::Placeholder;
  bool drawingShown_ = false;
  std::unique_ptr<uint8_t[]> thumb_;
  int thumbW_ = 0;
  int thumbH_ = 0;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  struct Layout {
    int top, bottom, winX, winY, winW, winH, thumbX, thumbY;
  };
  Layout layout() const;
  void step(int delta);
  void startPreview();
  void computePreview();
  void apply();

 public:
  SleepScreenPickerActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("SleepScreenPicker", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
