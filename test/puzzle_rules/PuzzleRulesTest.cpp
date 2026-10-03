#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <utility>

#include "activities/home/PuzzleRules.h"

namespace {
void solvedBoard(uint8_t tiles[16]) {
  for (int i = 0; i < 16; ++i) tiles[i] = static_cast<uint8_t>((i + 1) % 16);
}

int blankIndex(const uint8_t tiles[16]) {
  for (int i = 0; i < 16; ++i)
    if (tiles[i] == 0) return i;
  return -1;
}
}  // namespace

TEST(PuzzleRules, GoalBoardIsSolvable) {
  uint8_t tiles[16];
  solvedBoard(tiles);
  EXPECT_TRUE(PuzzleRules::solvable(tiles));
}

TEST(PuzzleRules, SwappedFourteenFifteenIsNotSolvable) {
  // The classic impossible position: the goal with 14 and 15 swapped.
  uint8_t tiles[16];
  solvedBoard(tiles);
  std::swap(tiles[13], tiles[14]);
  EXPECT_FALSE(PuzzleRules::solvable(tiles));
}

TEST(PuzzleRules, FullyReversedBoardIsOnTheUnreachableSide) {
  // 15, 14, ... 1 with the blank last: 105 inversions (odd), blank on the
  // bottom row, so it cannot be reached from the goal. The old shuffle had
  // this parity backwards, so players only ever got boards from this side,
  // where the "decreasing rows" arrangement was the only thing they could finish.
  uint8_t tiles[16];
  for (int i = 0; i < 15; ++i) tiles[i] = static_cast<uint8_t>(15 - i);
  tiles[15] = 0;
  EXPECT_FALSE(PuzzleRules::solvable(tiles));
}

TEST(PuzzleRules, EverySlideKeepsTheBoardSolvable) {
  std::mt19937 rng(12345);
  uint8_t tiles[16];
  solvedBoard(tiles);
  for (int step = 0; step < 20000; ++step) {
    const int blank = blankIndex(tiles);
    const int row = blank / 4, col = blank % 4;
    int options[4], count = 0;
    if (row > 0) options[count++] = blank - 4;
    if (row < 3) options[count++] = blank + 4;
    if (col > 0) options[count++] = blank - 1;
    if (col < 3) options[count++] = blank + 1;
    std::swap(tiles[blank], tiles[options[rng() % count]]);
    ASSERT_TRUE(PuzzleRules::solvable(tiles)) << "after " << step << " slides";
  }
}

TEST(PuzzleRules, SwappingTwoTilesFlipsSolvability) {
  std::mt19937 rng(7);
  uint8_t tiles[16];
  solvedBoard(tiles);
  for (int i = 15; i > 0; --i) std::swap(tiles[i], tiles[rng() % (i + 1)]);
  const bool before = PuzzleRules::solvable(tiles);
  int a = -1, b = -1;
  for (int i = 0; i < 16 && b < 0; ++i) {
    if (tiles[i] == 0) continue;
    if (a < 0) a = i;
    else b = i;
  }
  std::swap(tiles[a], tiles[b]);
  EXPECT_NE(before, PuzzleRules::solvable(tiles));
}
