#include "MoonPhase.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>

#include "images/MoonTexture.h"

namespace MoonPhase {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSynodicMonthDays = 29.530588853;
// New moon 2000-01-06 18:14 UTC, the commonly-cited reference epoch for this
// calculation.
constexpr double kReferenceNewMoonJD = 2451550.26;

// Fractional Julian Day for a UTC calendar date/time, via the standard
// Fliegel & Van Flandern integer algorithm plus a time-of-day fraction. JDN
// counts from noon UTC, hence the (hour-12) term.
double julianDay(int year, int month, int day, int hour, int minute) {
  const int a = (14 - month) / 12;
  const int y = year + 4800 - a;
  const int m = month + 12 * a - 3;
  const long jdn = day + (153L * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
  const double dayFraction = (hour - 12) / 24.0 + minute / 1440.0;
  return static_cast<double>(jdn) + dayFraction;
}

// Nearest-neighbor sample of the embedded moon photo, returning 0-15 (the
// texture is stored at 4 bits per sample with its contrast already baked in;
// see MoonTexture.h). u, v in [0, 1] map across the full kMoonTextureSize
// square, which is cropped tight to the disc -- so (u, v) here should come
// from the same [-1, 1] disc-normalized (nx, ny) the terminator test below
// uses, rescaled to [0, 1]. Nearest rather than bilinear: one array read
// instead of four plus interpolation, real per-pixel savings, and once the
// result is dithered to 1-bit the difference from bilinear isn't visible.
float sampleMoonTexture(float u, float v) {
  const int x = static_cast<int>(std::clamp(u, 0.0f, 1.0f) * (kMoonTextureSize - 1) + 0.5f);
  const int y = static_cast<int>(std::clamp(v, 0.0f, 1.0f) * (kMoonTextureSize - 1) + 0.5f);
  return static_cast<float>(moonSample(x, y));
}
}  // namespace

float phaseFraction(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
  const double jd = julianDay(year, month, day, hour, minute);
  double phase = std::fmod(jd - kReferenceNewMoonJD, kSynodicMonthDays) / kSynodicMonthDays;
  if (phase < 0.0) phase += 1.0;
  return static_cast<float>(phase);
}

const char* phaseName(float fraction) {
  const int bucket = static_cast<int>(std::floor(fraction * 8.0f + 0.5f)) % 8;
  switch (bucket) {
    case 0:
      return tr(STR_MOON_NEW);
    case 1:
      return tr(STR_MOON_WAXING_CRESCENT);
    case 2:
      return tr(STR_MOON_FIRST_QUARTER);
    case 3:
      return tr(STR_MOON_WAXING_GIBBOUS);
    case 4:
      return tr(STR_MOON_FULL);
    case 5:
      return tr(STR_MOON_WANING_GIBBOUS);
    case 6:
      return tr(STR_MOON_LAST_QUARTER);
    default:
      return tr(STR_MOON_WANING_CRESCENT);
  }
}

int illuminationPercent(float fraction) {
  // Same cosine illumination model as the terminator geometry below.
  const double illum = (1.0 - std::cos(2.0 * kPi * static_cast<double>(fraction))) / 2.0;
  return static_cast<int>(std::lround(illum * 100.0));
}

DiscTexture::DiscTexture(int radiusIn, float fraction) : radius_(std::max(0, radiusIn)) {
  width_ = 2 * radius_ + 1;
  bits_.assign((static_cast<size_t>(width_) * width_ + 7) / 8, 0);
  if (radius_ <= 0) return;

  // The illumination boundary is computed purely geometrically -- a curve
  // on the unit disc -- entirely independent of the surface texture sampled
  // below, so surface detail can never distort it into the large irregular
  // lobes a texture-driven brightness threshold produced before. Mirrors
  // around fraction 0.5 (full moon): the same cosine shape sweeps the lit
  // side from right (waxing) to left (waning), matching e.g. First Quarter
  // showing right-lit and Last Quarter showing left-lit.
  const float fEff = 0.5f - std::fabs(fraction - 0.5f);
  const float k = static_cast<float>(std::cos(2.0 * kPi * static_cast<double>(fEff)));
  const float sign = fraction < 0.5f ? 1.0f : -1.0f;
  const int r2 = radius_ * radius_;
  // Division is one of the more expensive operations in software float on a
  // chip with no hardware FPU (ESP32-C3); computed once and reused as a
  // multiply for every nx/ny below instead of dividing by radius per pixel.
  const float invRadius = 1.0f / static_cast<float>(radius_);
  constexpr float kInvMaxSample = 1.0f / 15.0f;

  // Floyd-Steinberg error diffusion needs only the current and next row (it
  // never propagates further than y+1), so a 2-row sliding buffer suffices
  // -- reset per call, so a call spanning the whole disc (draw(), the
  // common case) dithers with no seams; a banded call from the reveal
  // animation may show a faint seam at a band boundary.
  std::vector<float> err0(width_, 0.0f);
  std::vector<float> err1(width_, 0.0f);

  for (int dy = -radius_; dy <= radius_; ++dy) {
    const float ny = static_cast<float>(dy) * invRadius;
    // w (the local disc half-width for this row) only depends on ny, so this
    // sqrt runs once per row rather than once per pixel.
    const float w = std::sqrt(std::max(0.0f, 1.0f - ny * ny));
    const float v = (ny + 1.0f) * 0.5f;
    std::fill(err1.begin(), err1.end(), 0.0f);

    for (int dx = -radius_; dx <= radius_; ++dx) {
      const int col = dx + radius_;
      if (dx * dx + dy * dy > r2) continue;
      const float nx = static_cast<float>(dx) * invRadius;
      const bool lit = (sign * nx) >= (k * w - 1e-4f);

      // The unlit hemisphere is rendered solid black, not textured or
      // shaded -- it really is in shadow, and stippling it would just be
      // procedural texture again by another name.
      float brightness = 0.0f;
      if (lit) {
        const float u = (nx + 1.0f) * 0.5f;
        brightness = sampleMoonTexture(u, v) * kInvMaxSample;
      }
      brightness = std::clamp(brightness + err0[col], 0.0f, 1.0f);

      const bool ink = brightness < 0.5f;
      if (ink) {
        const size_t idx = static_cast<size_t>(dy + radius_) * width_ + col;
        bits_[idx / 8] |= static_cast<uint8_t>(1u << (idx % 8));
      }
      const float output = ink ? 0.0f : 1.0f;
      const float error = brightness - output;

      // Classic Floyd-Steinberg kernel: 7/16 right, 3/16 below-left, 5/16
      // below, 1/16 below-right.
      if (col + 1 < width_) err0[col + 1] += error * (7.0f / 16.0f);
      if (col - 1 >= 0) err1[col - 1] += error * (3.0f / 16.0f);
      err1[col] += error * (5.0f / 16.0f);
      if (col + 1 < width_) err1[col + 1] += error * (1.0f / 16.0f);
    }
    std::swap(err0, err1);
  }
}

bool DiscTexture::ink(int dx, int dy) const {
  if (radius_ <= 0) return false;
  const int col = dx + radius_;
  const int row = dy + radius_;
  if (col < 0 || col >= width_ || row < 0 || row >= width_) return false;
  const size_t idx = static_cast<size_t>(row) * width_ + col;
  return (bits_[idx / 8] & static_cast<uint8_t>(1u << (idx % 8))) != 0;
}

void draw(const GfxRenderer& renderer, int cx, int cy, const DiscTexture& texture) {
  drawRows(renderer, cx, cy, texture, cy - texture.radius(), cy + texture.radius() + 1);
}

void drawRows(const GfxRenderer& renderer, int cx, int cy, const DiscTexture& texture, int yStart, int yEnd) {
  const int radius = texture.radius();
  const int rowLo = std::max(yStart, cy - radius);
  const int rowHi = std::min(yEnd, cy + radius + 1);
  for (int y = rowLo; y < rowHi; ++y) {
    const int dy = y - cy;
    for (int dx = -radius; dx <= radius; ++dx) {
      if (texture.ink(dx, dy)) renderer.drawPixel(cx + dx, y, true);
    }
  }
}

}  // namespace MoonPhase
