#include "RockerSwitch.h"

#include <GfxRenderer.h>

#include <algorithm>

namespace {
constexpr int kKnob = RockerSwitch::kHeight - 2 * RockerSwitch::kKnobInset;

struct Pending {
  bool valid = false;
  int x = 0;
  int y = 0;
};
Pending pending;

// Pixel symbols, so they stay crisp whatever the UI font is.
void drawI(const GfxRenderer& r, const int cx, const int cy, const bool ink) {
  r.fillRect(cx - 2, cy - 6, 4, 12, ink);
}

void drawO(const GfxRenderer& r, const int cx, const int cy, const bool ink) {
  r.drawRect(cx - 5, cy - 6, 10, 12, 3, ink);
}
}  // namespace

namespace RockerSwitch {
void paint(const GfxRenderer& r, const int x, const int y, const bool on) {
  // Opaque backing so a selected (inverted) row can't change how it reads.
  r.fillRect(x - 2, y - 2, kWidth + 4, kHeight + 4, false);
  // Housing: a grey plate the lever tilts in, with a heavy outline.
  r.fillRectDither(x, y, kWidth, kHeight, Color::LightGray);
  r.drawRect(x, y, kWidth, kHeight, 2, true);

  constexpr int kTilt = 4;  // how far the two halves sit apart vertically
  const int innerTop = y + 3;
  const int innerBottom = y + kHeight - 3;
  const int halfW = (kWidth - 6) / 2;
  const int halfH = innerBottom - innerTop - kTilt;
  const int leftX = x + 3;
  const int rightX = x + 3 + halfW + 1;
  // The pressed half sits low and solid; the raised half stands up with a
  // heavy underside, so the switch visibly tilts toward the pressed side.
  const int leftY = on ? innerTop : innerTop + kTilt;
  const int rightY = on ? innerTop + kTilt : innerTop;

  auto half = [&](const int hx, const int hy, const bool pressed, const bool isOn) {
    if (pressed) {
      r.fillRect(hx, hy, halfW, halfH, true);
    } else {
      r.fillRect(hx, hy, halfW, halfH, false);
      r.drawRect(hx, hy, halfW, halfH, true);
      r.fillRect(hx, hy + halfH - 4, halfW, 4, true);  // underside lip
    }
    const int faceH = pressed ? halfH : halfH - 4;
    const int cx = hx + halfW / 2;
    const int cy = hy + faceH / 2;
    if (isOn) {
      drawI(r, cx, cy, !pressed);
    } else {
      drawO(r, cx, cy, !pressed);
    }
  };
  half(leftX, leftY, !on, false);   // "O" is pressed when off
  half(rightX, rightY, on, true);   // "I" is pressed when on
}
}  // namespace RockerSwitch

namespace {
void onFill(const GfxRenderer& r, const int x, const int y, const int w, const int h, bool) {
  if (w == RockerSwitch::kWidth && h == RockerSwitch::kHeight) {
    pending = {true, x, y};
    return;
  }
  if (!pending.valid || w != kKnob || h != kKnob) return;
  const int knobOffset = x - pending.x;
  if (y < pending.y || y >= pending.y + RockerSwitch::kHeight) return;
  const Pending track = pending;
  pending.valid = false;
  // The knob sits at the left edge when off and the right edge when on.
  RockerSwitch::paint(r, track.x, track.y, knobOffset > RockerSwitch::kWidth / 2 - kKnob / 2);
}
}  // namespace

namespace RockerSwitch {
void setEnabled(const bool enabled) {
  pending.valid = false;
  GfxRenderer::setFillObserver(enabled ? &onFill : nullptr);
}
}  // namespace RockerSwitch
