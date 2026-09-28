#include "DesktopPattern.h"

#include <GfxRenderer.h>

#include <algorithm>

namespace DesktopPattern {

namespace {
// One byte per row, most significant bit on the left. The order matches the
// Desktop Pattern setting's option list.
constexpr uint8_t kPatterns[kCount][8] = {
    {0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33},  // Checker (2x2 blocks; the original RetroInk desktop)
    {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55},  // Gray 50%
    {0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22},  // Gray 25%
    {0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD},  // Gray 75%
    {0xFF, 0x80, 0x80, 0x80, 0xFF, 0x08, 0x08, 0x08},  // Brick
    {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x81},  // Diagonal
    {0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00},  // Horizontal Lines
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88},  // Vertical Lines
    {0xF8, 0x74, 0x22, 0x47, 0x8F, 0x17, 0x22, 0x74},  // Weave
    {0xFF, 0x80, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80},  // Grid
    {0x10, 0x28, 0x44, 0x82, 0x44, 0x28, 0x10, 0x00},  // Diamonds
    {0x80, 0x80, 0x41, 0x3E, 0x08, 0x08, 0x14, 0xE3},  // Scales
    {0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00},  // Dots
    {0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00},  // Polka
    {0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0},  // Stripes
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // White
};
}  // namespace

void fill(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
          const uint8_t pattern) {
  if (width <= 0 || height <= 0) return;
  const uint8_t* rows = kPatterns[pattern < kCount ? pattern : kDefault];
  const int right = x + width;
  const int bottom = y + height;

  bool anyInk = false;
  bool identicalRows = true;
  for (int i = 0; i < 8; ++i) {
    anyInk = anyInk || rows[i] != 0;
    identicalRows = identicalRows && rows[i] == rows[0];
  }
  if (!anyInk) return;

  // Every row the same (vertical stripes): one tall thin rectangle per ink
  // column instead of one per row.
  if (identicalRows) {
    for (int bit = 0; bit < 8; ++bit) {
      if (!((rows[0] >> (7 - bit)) & 1)) continue;
      for (int px = (x & ~7) + bit; px < right; px += 8) {
        if (px >= x) renderer.fillRect(px, y, 1, height);
      }
    }
    return;
  }

  for (int py = y; py < bottom;) {
    const uint8_t row = rows[py & 7];
    // Fold neighbouring identical rows into one taller band.
    int bandHeight = 1;
    while (py + bandHeight < bottom && rows[(py + bandHeight) & 7] == row && bandHeight < 8) ++bandHeight;
    if (row == 0) {
      py += bandHeight;
      continue;
    }
    if (row == 0xFF) {
      renderer.fillRect(x, py, width, bandHeight);
      py += bandHeight;
      continue;
    }
    for (int tile = x & ~7; tile < right; tile += 8) {
      for (int bit = 0; bit < 8;) {
        if (!((row >> (7 - bit)) & 1)) {
          ++bit;
          continue;
        }
        int runEnd = bit;
        while (runEnd + 1 < 8 && ((row >> (7 - (runEnd + 1))) & 1)) ++runEnd;
        const int runStart = std::max(tile + bit, x);
        const int runStop = std::min(tile + runEnd + 1, right);
        if (runStop > runStart) renderer.fillRect(runStart, py, runStop - runStart, bandHeight);
        bit = runEnd + 1;
      }
    }
    py += bandHeight;
  }
}

}  // namespace DesktopPattern
