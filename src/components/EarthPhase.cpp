#include "EarthPhase.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cmath>

#include "images/EarthTexture.h"

namespace EarthPhase {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kAxialTiltDeg = 23.44f;

// Ordered 4x4 Bayer thresholds scaled to 0-255, so the whole per-pixel shading
// decision stays in integers.
constexpr uint8_t kBayer[16] = {8, 136, 40, 168, 200, 72, 232, 104, 56, 184, 24, 152, 248, 120, 216, 88};

// Ink for sea-level land (dense) through the highest peaks (light), and how
// much darker the night side gets. Night land is only nudged: enough that the
// terminator stays visible where it crosses a continent, not so much that it
// swallows the topography.
constexpr int kLowlandInk = 235;
constexpr int kElevationRange = 184;
constexpr int kNightLandBoost = 38;

// Night ocean is ruled every other row.
constexpr int kNightOceanRule = 2;

int dayOfYear(const uint16_t year, const uint8_t month, const uint8_t day) {
  static constexpr int kCumulative[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  const int m = std::clamp<int>(month, 1, 12);
  return kCumulative[m - 1] + day + ((leap && m > 2) ? 1 : 0);
}

int wrapColumn(int column) {
  while (column < 0) column += EarthTexture::kMaskWidth;
  while (column >= EarthTexture::kMaskWidth) column -= EarthTexture::kMaskWidth;
  return column;
}

bool isCoast(const int column, const int row) {
  if (!EarthTexture::isLand(column, row)) return false;
  const int up = std::max(0, row - 1);
  const int down = std::min(EarthTexture::kMaskHeight - 1, row + 1);
  return !EarthTexture::isLand(wrapColumn(column - 1), row) || !EarthTexture::isLand(wrapColumn(column + 1), row) ||
         !EarthTexture::isLand(column, up) || !EarthTexture::isLand(column, down);
}
}  // namespace

void subsolar(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t hour, const uint8_t minute,
              float& declinationRad, float& subsolarLonRad) {
  const int doy = dayOfYear(year, month, day);
  // Declination peaks at the solstices and crosses zero at the equinoxes; the
  // +10 offset puts the minimum near the December solstice.
  const float seasonAngle = 2.0f * kPi / 365.0f * static_cast<float>(doy + 10);
  declinationRad = -kAxialTiltDeg * (kPi / 180.0f) * std::cos(seasonAngle);
  // The sun stands over the meridian where it is local noon: longitude 0 at
  // 12:00 UTC, moving 15 degrees west per hour.
  const float utcHours = static_cast<float>(hour) + static_cast<float>(minute) / 60.0f;
  subsolarLonRad = (15.0f * (12.0f - utcHours)) * (kPi / 180.0f);
}

float centerLonForUtcOffsetQ(const uint8_t clockUtcOffsetQ) {
  const float offsetHours = (static_cast<int>(clockUtcOffsetQ) - 48) / 4.0f;
  return offsetHours * 15.0f * (kPi / 180.0f);
}

DiscTexture::DiscTexture(const int radiusIn, const float centerLonRad, const float declinationRad,
                         const float subsolarLonRad)
    : radius_(std::max(0, radiusIn)) {
  width_ = 2 * radius_ + 1;
  bits_.assign((static_cast<size_t>(width_) * width_ + 7) / 8, 0);
  if (radius_ <= 0) return;

  const int r2 = radius_ * radius_;
  const float invRadius = 1.0f / static_cast<float>(radius_);
  const float delta = centerLonRad - subsolarLonRad;
  const float cosDelta = std::cos(delta);
  const float sinDelta = std::sin(delta);
  const float sinDecl = std::sin(declinationRad);
  const float cosDecl = std::cos(declinationRad);

  // Longitude offset of each visible map column, as sin(dlon). This depends
  // only on the column, never the row, so it is built once for the whole disc.
  constexpr int kHalfCols = EarthTexture::kMaskWidth / 4;  // a quarter turn = 90 degrees
  std::vector<float> sinDlon(2 * kHalfCols + 1);
  for (int j = -kHalfCols; j <= kHalfCols; ++j)
    sinDlon[j + kHalfCols] = std::sin(static_cast<float>(j) * 2.0f * kPi / EarthTexture::kMaskWidth);

  const int centerColumn =
      wrapColumn(static_cast<int>(std::lround((centerLonRad / (2.0f * kPi) + 0.5f) * EarthTexture::kMaskWidth)));

  // Screen column -> map column for the current row. Filled by walking the map
  // columns forward, which avoids an inverse trig call per pixel: near the
  // centre one map column covers several pixels, near the limb many collapse
  // onto one.
  std::vector<int16_t> columnForX(width_);

  for (int dy = -radius_; dy <= radius_; ++dy) {
    const float ny = static_cast<float>(dy) * invRadius;
    const float cosLat = std::sqrt(std::max(0.0f, 1.0f - ny * ny));
    const float sinLat = -ny;
    const int mapRow = std::clamp(
        static_cast<int>(std::lround((0.5f - std::asin(std::clamp(sinLat, -1.0f, 1.0f)) / kPi) *
                                     (EarthTexture::kMaskHeight - 1))),
        0, EarthTexture::kMaskHeight - 1);

    // Per-row constants of cos(zenith) = sin(lat)sin(decl) + cos(lat)cos(decl)cos(lon - lonSun).
    const float A = sinLat * sinDecl;
    const float B = cosLat * cosDecl;

    const bool degenerate = cosLat < 1e-4f;
    if (degenerate) {
      std::fill(columnForX.begin(), columnForX.end(), static_cast<int16_t>(centerColumn));
    } else {
      const float rowSpan = static_cast<float>(radius_) * cosLat;
      int previousX = -radius_ - 1;
      for (int j = -kHalfCols; j <= kHalfCols; ++j) {
        int x = static_cast<int>(std::lround(rowSpan * sinDlon[j + kHalfCols]));
        x = std::clamp(x, -radius_, radius_);
        const int16_t column = static_cast<int16_t>(wrapColumn(centerColumn + j));
        for (int fill = std::max(-radius_, previousX + 1); fill <= x; ++fill) columnForX[fill + radius_] = column;
        previousX = std::max(previousX, x);
      }
      for (int fill = std::max(-radius_, previousX + 1); fill <= radius_; ++fill)
        columnForX[fill + radius_] = static_cast<int16_t>(wrapColumn(centerColumn + kHalfCols));
    }

    const float invCosLat = degenerate ? 0.0f : 1.0f / cosLat;
    const int py = dy + radius_;

    for (int dx = -radius_; dx <= radius_; ++dx) {
      if (dx * dx + dy * dy > r2) continue;
      const int px = dx + radius_;

      // sin/cos of the longitude offset. cos is always positive because only
      // the near hemisphere is visible, so no quadrant test is needed.
      const float s = degenerate ? 0.0f : std::clamp(static_cast<float>(dx) * invRadius * invCosLat, -1.0f, 1.0f);
      const float c = std::sqrt(std::max(0.0f, 1.0f - s * s));
      const bool daylight = (A + B * (c * cosDelta - s * sinDelta)) > 0.0f;

      const int column = columnForX[px];
      bool ink;
      if (!EarthTexture::isLand(column, mapRow)) {
        // Day ocean is blank; night ocean is ruled with horizontal lines.
        ink = daylight ? false : (py % kNightOceanRule == 0);
      } else if (isCoast(column, mapRow)) {
        ink = true;
      } else {
        const uint8_t height = EarthTexture::elevation(column * EarthTexture::kElevWidth / EarthTexture::kMaskWidth,
                                                       mapRow * EarthTexture::kElevHeight / EarthTexture::kMaskHeight);
        int t = kLowlandInk - kElevationRange * static_cast<int>(height) / 15;
        if (!daylight) t = std::min(255, t + kNightLandBoost);
        ink = t > kBayer[(py & 3) * 4 + (px & 3)];
      }

      if (ink) {
        const size_t index = static_cast<size_t>(py) * width_ + px;
        bits_[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
      }
    }
  }
}

bool DiscTexture::ink(const int dx, const int dy) const {
  if (radius_ <= 0) return false;
  const int col = dx + radius_;
  const int row = dy + radius_;
  if (col < 0 || col >= width_ || row < 0 || row >= width_) return false;
  const size_t index = static_cast<size_t>(row) * width_ + col;
  return (bits_[index / 8] & static_cast<uint8_t>(1u << (index % 8))) != 0;
}

void draw(const GfxRenderer& renderer, const int cx, const int cy, const DiscTexture& texture) {
  const int radius = texture.radius();
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      if (texture.ink(dx, dy)) renderer.drawPixel(cx + dx, cy + dy, true);
    }
  }
  // A rim keeps the disc reading as a globe where the day-side ocean meets the
  // background, both being blank.
  renderer.drawArc(radius, cx, cy, -1, -1, 1, true);
  renderer.drawArc(radius, cx, cy, -1, 1, 1, true);
  renderer.drawArc(radius, cx, cy, 1, -1, 1, true);
  renderer.drawArc(radius, cx, cy, 1, 1, 1, true);
}

}  // namespace EarthPhase
