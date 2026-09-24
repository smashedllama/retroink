#pragma once

#include <cstdint>
#include <vector>

class GfxRenderer;

// A day/night Earth globe: an orthographic hemisphere centred on the user's
// timezone, with the terminator swept by the time of day and tilted by the
// time of year, drawn over real topography (see images/EarthTexture.h).
//
// Land is shaded by elevation -- lowlands dense, peaks light, like snow on the
// ridges -- while the night ocean is horizontally hatched. The hatch is
// deliberately a different *structure* from the land's stipple rather than
// just a different density: at matching densities the two are indistinguishable
// and the continents dissolve into the night side.
namespace EarthPhase {

// Sun position for a UTC date/time. Declination gives the seasonal tilt of the
// terminator; the subsolar longitude gives its rotation through the day.
void subsolar(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, float& declinationRad,
              float& subsolarLonRad);

// Longitude the globe should be centred on for a UTC offset in quarter-hours
// (CrossPointSettings::clockUtcOffsetQ's raw encoding).
float centerLonForUtcOffsetQ(uint8_t clockUtcOffsetQ);

// A precomputed, dithered globe. Building one costs real per-pixel work, so
// build it once and blit it for every draw rather than recomputing per frame.
class DiscTexture {
 public:
  DiscTexture(int radius, float centerLonRad, float declinationRad, float subsolarLonRad);

  int radius() const { return radius_; }
  // Ink (black) at (dx, dy) relative to the disc centre.
  bool ink(int dx, int dy) const;

 private:
  int radius_ = 0;
  int width_ = 0;
  std::vector<uint8_t> bits_;
};

// Blits the whole texture centred at (cx, cy) -- bit lookups only, cheap.
void draw(const GfxRenderer& renderer, int cx, int cy, const DiscTexture& texture);

}  // namespace EarthPhase
