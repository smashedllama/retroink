#pragma once

#include <IcsCalendar.h>

#include <GfxRenderer.h>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// A static desk-calendar month view, paged with Left/Right. Today's cell is
// highlighted when the viewed month contains it.
class DeskCalendarActivity final : public Activity {
  int viewYear_ = 2026;
  int viewMonth_ = 1;  // 1-12
  bool todayKnown_ = false;
  int todayYear_ = 0, todayMonth_ = 0, todayDay_ = 0;
  // Without a clock chip, "today" is the date the user picked (DeskDate), and
  // Confirm picks it again; with no saved date the picker opens first.
  bool picksDate_ = false;
  bool pickerPending_ = false;
  bool exitPending_ = false;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;
  // Events from the synced calendar feed, if there is one. With events, the
  // side buttons (or a tap) pick a day and its agenda shows under the grid.
  ics::Calendar events_;
  bool hasEvents_ = false;
  int selectedDay_ = 0;  // 1-31 within the viewed month, 0 = none

  void moveSelection(int deltaDays);
  Rect gridRect() const;

  void pageMonth(int delta);
  void showToday();
  void openPicker();

 public:
  DeskCalendarActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("DeskCalendar", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
