#pragma once

#include <IcsCalendar.h>

#include <GfxRenderer.h>
#include <vector>

#include "activities/Activity.h"

// One day of the synced calendar, full screen: every event on its own row
// with the time on the left and the whole title wrapped beside it. Front
// Left/Right move to the previous/next day, the side buttons (or a swipe)
// scroll a long day, Back returns to the month.
class DeskDayActivity final : public Activity {
  ics::Calendar events_;
  int32_t* day_;  // the day being shown; the caller reads it back when this closes
  std::vector<size_t> indices_;
  int first_ = 0;      // index into indices_ of the first row shown
  int visible_ = 0;    // rows that fit from first_ (set by render)
  int maxFirst_ = 0;   // furthest the list can scroll (set by render)
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  void loadDay();
  void changeDay(int delta);
  void scroll(int delta);

 public:
  DeskDayActivity(GfxRenderer& renderer, MappedInputManager& input, ics::Calendar events, int32_t* day)
      : Activity("DeskDay", renderer, input), events_(std::move(events)), day_(day) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
