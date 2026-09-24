#pragma once

#include "activities/Activity.h"

// A static desk-calendar month view, paged with Left/Right. Today's cell is
// highlighted when the viewed month contains it.
class DeskCalendarActivity final : public Activity {
  int viewYear_ = 2026;
  int viewMonth_ = 1;  // 1-12
  bool todayKnown_ = false;
  int todayYear_ = 0, todayMonth_ = 0, todayDay_ = 0;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  void pageMonth(int delta);

 public:
  DeskCalendarActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("DeskCalendar", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
