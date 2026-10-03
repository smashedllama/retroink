#pragma once

#include <cstdint>

// Rules for the 4x4 sliding Puzzle, kept free of any device code so they can
// be tested on a computer.
namespace PuzzleRules {

// tiles[position] = tile number 1-15, 0 = blank; positions run left to right,
// top to bottom. The goal is 1..15 in order with the blank in the last position.
//
// For a board with an even width, a position can be reached from the goal
// exactly when (inversions + the blank's row counted from the bottom, starting
// at 1) is odd. Counting inversions among the tiles only, ignoring the blank.
inline bool solvable(const uint8_t tiles[16]) {
  int inversions = 0;
  int blankPos = 15;
  for (int i = 0; i < 16; ++i) {
    if (tiles[i] == 0) {
      blankPos = i;
      continue;
    }
    for (int j = i + 1; j < 16; ++j) {
      if (tiles[j] != 0 && tiles[i] > tiles[j]) ++inversions;
    }
  }
  const int blankRowFromBottom = 4 - blankPos / 4;
  return (inversions + blankRowFromBottom) % 2 == 1;
}

}  // namespace PuzzleRules
