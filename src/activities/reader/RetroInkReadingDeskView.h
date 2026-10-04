#pragma once

#include <string>

#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

class GfxRenderer;
class MappedInputManager;

namespace RetroInkReadingDeskView {
// The book to show in the Today sleep screen's "Now Reading" card.
struct NowReading {
  std::string title;
  float progressPercent = 0.0f;
  std::string coverBmpPath;  // may be empty (draws a placeholder box instead)
};
// With `nowReading`, the card fills the space under the week strip (the sleep
// screen); the Reading Desk passes none and keeps its shorter layout.
void renderToday(GfxRenderer& renderer, const MappedInputManager* input, const GlobalReadingStats& globalStats,
                 const NowReading* nowReading = nullptr);
void renderYear(GfxRenderer& renderer, const MappedInputManager* input);
void renderBookStatus(GfxRenderer& renderer, const MappedInputManager* input, const std::string& title,
                      const BookReadingStats& stats, float progressPercent, uint32_t estimatedTimeLeftSeconds);
// The sleep screen version of Book Status: the cover beside the title, and a
// longer list of numbers (reading time, pages, sessions, start or finish date,
// time left) spread over the page. coverBmpPath may be empty.
void renderBookStatusSleep(GfxRenderer& renderer, const std::string& title, const BookReadingStats& stats,
                           float progressPercent, uint32_t estimatedTimeLeftSeconds, const std::string& coverBmpPath);
// Cover + title + a per-weekday reading-time bar chart for one book.
// coverBmpPath may be empty (draws a placeholder box instead).
void renderBookWeekStatus(GfxRenderer& renderer, const MappedInputManager* input, const std::string& title,
                          const BookReadingStats& stats, const std::string& coverBmpPath);
}  // namespace RetroInkReadingDeskView
