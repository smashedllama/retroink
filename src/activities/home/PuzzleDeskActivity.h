#pragma once

#include <cstdint>

#include "activities/Activity.h"

// The classic Mac "Puzzle" desk accessory: a 15-tile slider. Arrow buttons
// slide the tile in that direction into the blank; touch devices can also
// tap any tile adjacent to the blank directly. Confirm reshuffles. The board
// is saved after every move, so closing it (or the device sleeping mid-game)
// resumes where you left off rather than starting over.
class PuzzleDeskActivity final : public Activity {
  uint8_t tiles_[16] = {};  // position -> tile number, 0 = blank
  int blankPos_ = 15;
  uint32_t moves_ = 0;
  bool solved_ = false;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  void shuffle();
  bool isSolvable() const;
  void moveBlank(int dRow, int dCol);
  void checkSolved();
  void gridGeometry(int& cellSize, int& gridLeft, int& gridTop) const;

  // Board + move count, persisted to /.crosspoint/puzzle.bin. loadState()
  // returns false (leaving the board untouched) when there's no save, it's
  // from another format version, or the stored tiles aren't a valid
  // permutation -- the caller then shuffles a fresh board instead.
  bool loadState();
  void saveState() const;

 public:
  PuzzleDeskActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("PuzzleDesk", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
