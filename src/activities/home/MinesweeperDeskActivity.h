#pragma once

#include <cstdint>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

// Minesweeper for touch screens. A tap digs a square, or plants a flag while the
// Flag key is on; tapping a number whose flags are all placed clears around it.
// New Game lives in the Menu so it can't be hit by accident. The first dig is
// never a mine, and the game isn't saved: a round is short enough to restart.
class MinesweeperDeskActivity final : public Activity {
 public:
  static constexpr int kCols = 8;
  static constexpr int kRows = 12;
  static constexpr int kCells = kCols * kRows;
  static constexpr int kMines = 16;

  MinesweeperDeskActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("MinesweeperDesk", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Flag : uint8_t { Mine = 1, Revealed = 2, Flagged = 4 };

  uint8_t cell_[kCells] = {};
  uint8_t near_[kCells] = {};  // mines in the 8 neighbours
  bool minesPlaced_ = false;
  bool flagMode_ = false;
  OptionPopup menu_;
  bool lost_ = false;
  bool won_ = false;
  int hitCell_ = -1;  // the mine that ended the game
  unsigned long startedMs_ = 0;
  uint32_t seconds_ = 0;
  GfxRenderer::Orientation previousOrientation_ = GfxRenderer::Orientation::Portrait;

  void newGame();
  void placeMines(int safeCell);
  void dig(int index);
  void chord(int index);
  void reveal(int index);
  void toggleFlag(int index);
  void checkWon();
  int flagsPlaced() const;
  int cellAt(int x, int y) const;
  void geometry(int& cellSize, int& left, int& top) const;
  void drawCell(int index, int x, int y, int size) const;
};
