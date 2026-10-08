#pragma once

#include <I18n.h>

#include "activities/Activity.h"
#include "components/DeskDate.h"
#include "util/ButtonNavigator.h"

// Picks the date the Moon Phase, Earth, and Desk Calendar accessories show
// (see DeskDate.h). Fields are a vertical list: the side buttons move between
// them and the front buttons change the selected value, holding to scroll.
// Done saves the result to the SD card; Back cancels without saving.
class DeskDatePickerActivity final : public Activity {
 public:
  // Earth also needs a time of day and a time zone; the moon and calendar
  // only need the date.
  enum class Fields : uint8_t { DateOnly, DateTimeZone };

 private:
  enum class Field : uint8_t { Day, Month, Year, Hour, Minute, Zone };

  StrId titleId_;
  Fields fields_;
  DeskDateTime value_;
  int selected_ = 0;
  ButtonNavigator valueNavigator_{150, 450};

  // Touch: -/+ buttons at the end of each row, held to repeat.
  int repeatDelta_ = 0;
  uint32_t repeatStartMs_ = 0;
  uint32_t lastRepeatMs_ = 0;

  struct RowGeometry {
    int rowX, rowW, rowH, firstY, step, buttonW;
  };
  RowGeometry rowGeometry() const;
  // Returns the row and, if the point is on its - or + button, -1 or +1 (else 0); -1 row if none.
  int rowAt(int x, int y, int& buttonDelta) const;
  void handleTouch();

  int fieldCount() const { return fields_ == Fields::DateTimeZone ? 6 : 3; }
  void adjust(int delta);
  void formatValue(Field field, char* buf, size_t len) const;

 public:
  DeskDatePickerActivity(GfxRenderer& renderer, MappedInputManager& input, StrId titleId, Fields fields,
                         const DeskDateTime& start)
      : Activity("DeskDatePicker", renderer, input), titleId_(titleId), fields_(fields), value_(start) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
