#pragma once

#include <cstdint>
#include <vector>

#include <IcsCalendar.h>

class GfxRenderer;
struct Rect;

// The System6 desk-calendar look (pinstriped month/year banner, weekday
// header, day grid with a badged "today") shared between DeskCalendarActivity
// and the matching Desk Calendar sleep screen, so the two can't drift apart.
namespace CalendarView {

// Optional extras for the month grid.
struct Markers {
  uint32_t eventMask = 0;  // bit (day - 1) set: draw an event dot under that day
  int selectedDay = 0;     // 1-31 draws a frame around that day; 0 = none
};

// Draws into `rect` -- no outer frame; callers draw their own chrome around
// this (DeskCalendarActivity's simple double-border frame vs. a sleep
// screen's desktop window).
void draw(const GfxRenderer& renderer, Rect rect, int year, int month, bool todayKnown, int todayYear,
          int todayMonth, int todayDay, const Markers& markers = Markers());

// The day (1-31) drawn at a point inside `rect` for the same year and month,
// or 0 if the point is not on a day.
int dayAt(const GfxRenderer& renderer, Rect rect, int year, int month, int x, int y);

// Height of one agenda line, for working out how many fit in a rect.
int agendaLineHeight(const GfxRenderer& renderer);

// A list of events, one per line ("9:00 AM  Dentist", "All day  Holiday"),
// as many as fit in `rect`. With `showDate` each line starts with a short
// month and day instead (for the upcoming list), except events on `timeOnDay`
// (today), which keep their time. Returns how many lines were drawn.
size_t drawAgenda(const GfxRenderer& renderer, Rect rect, const ics::Calendar& calendar,
                  const std::vector<size_t>& indices, bool showDate, int32_t timeOnDay = INT32_MIN);

}  // namespace CalendarView
