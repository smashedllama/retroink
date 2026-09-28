#pragma once

#include <cstdint>

class GfxRenderer;

// The System 6 desktop texture behind every RetroInk window, chosen under
// Settings > Display > Desktop Pattern. Each pattern is a classic 8x8 one-bit
// Macintosh pattern, tiled from the screen's own origin so a window drawn over
// part of it always lines up.
namespace DesktopPattern {

constexpr uint8_t kCount = 16;
constexpr uint8_t kDefault = 0;  // the original RetroInk checker

// Fills the rectangle with the pattern (black pixels only; the screen is
// expected to already be white).
void fill(const GfxRenderer& renderer, int x, int y, int width, int height, uint8_t pattern);

}  // namespace DesktopPattern
