#pragma once

#include <cstdint>
#include <vector>

class GfxRenderer;

// A real photographic moon phase: the disc is sampled from an embedded
// grayscale photograph (see src/images/MoonTexture.h), masked by a clean
// geometric terminator computed separately from the surface texture, then
// dithered to 1-bit with Floyd-Steinberg error diffusion -- the fine, even
// grain that actually matches the reference this was built against, as
// opposed to a coarser halftone-dot rendering that was tried and rejected
// as looking too different from it.
namespace MoonPhase {

// 0.0 = new moon, 0.5 = full moon, wrapping back toward 1.0 = new moon again.
// Expects the raw UTC RTC date/time, matching HalClock::getDateTime().
float phaseFraction(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute);

// One of the 8 classic phase names (New Moon, Waxing Crescent, ... Waning Crescent).
const char* phaseName(float fraction);

// Illuminated percentage, 0-100, for display alongside the disc.
int illuminationPercent(float fraction);

// A precomputed, dithered disc for one phase/radius. Building one is the
// expensive part (per-pixel texture sampling + error diffusion is real work
// on a chip with no hardware FPU) -- build it once and reuse it for every
// subsequent draw/reveal of the same phase, rather than recomputing per
// call. Owns a packed 1-bit-per-pixel buffer sized to the disc.
class DiscTexture {
 public:
  explicit DiscTexture(int radius, float fraction);

  int radius() const { return radius_; }
  // True if the pixel at (dx, dy) relative to the disc center is ink
  // (black). dx, dy must be within [-radius, radius].
  bool ink(int dx, int dy) const;

 private:
  int radius_;
  int width_;
  std::vector<uint8_t> bits_;
};

// Blits the whole texture centered at (cx, cy) -- no per-pixel computation,
// just bit lookups, so this is cheap and safe to call every render.
void draw(const GfxRenderer& renderer, int cx, int cy, const DiscTexture& texture);

// Blits only rows [yStart, yEnd) of the texture -- used to reveal the moon
// band-by-band via successive GfxRenderer::displayWindow() partial
// refreshes. Because the texture was already fully (and seamlessly)
// halftoned up front, blitting a sub-range never introduces a seam at the
// band boundary the way re-halftoning each band separately would.
void drawRows(const GfxRenderer& renderer, int cx, int cy, const DiscTexture& texture, int yStart, int yEnd);

}  // namespace MoonPhase
