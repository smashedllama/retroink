#pragma once

class GfxRenderer;
struct Rect;

// The System6 desk-calendar look (pinstriped month/year banner, weekday
// header, day grid with a badged "today") shared between DeskCalendarActivity
// and the matching Desk Calendar sleep screen, so the two can't drift apart.
namespace CalendarView {

// Draws into `rect` -- no outer frame; callers draw their own chrome around
// this (DeskCalendarActivity's simple double-border frame vs. a sleep
// screen's desktop window).
void draw(const GfxRenderer& renderer, Rect rect, int year, int month, bool todayKnown, int todayYear,
          int todayMonth, int todayDay);

}  // namespace CalendarView
