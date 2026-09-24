#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// A small submenu, in the classic-Mac "Desk Accessories" sense: Focus Timer
// plus the newer desk-accessory screens (Moon Phase, Earth, Clock, Puzzle,
// Desk Calendar, System Info), all nested here instead of flat in the Home menu.
class DeskAccessoriesActivity final : public Activity {
  enum class DeskAction : uint8_t { FocusTimer, MoonPhase, Earth, Clock, Puzzle, Calendar, SystemInfo };
  static constexpr int kItemCount = 7;

  int selected_ = 0;
  ButtonNavigator buttonNavigator_;

  void activate();

 public:
  DeskAccessoriesActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("DeskAccessories", renderer, input) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
