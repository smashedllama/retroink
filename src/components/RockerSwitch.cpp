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

}  // namespace

namespace RockerSwitch {
void paint(const GfxRenderer& r, const int x, const int y, const bool on) {
  // Opaque backing so a selected (inverted) row can't change how it reads.
  r.fillRect(x - 2, y - 2, kWidth + 4, kHeight + 4, false);

  // Bevelled bezel: a checkered frame with a heavier lower/right edge.
  r.fillRectDither(x, y, kWidth, kHeight, Color::LightGray);
  r.drawRect(x, y, kWidth, kHeight, true);
  r.drawLine(x + 1, y + kHeight - 1, x + kWidth - 1, y + kHeight - 1);
  r.drawLine(x + kWidth - 1, y + 1, x + kWidth - 1, y + kHeight - 1);

  // Dark well the two keys sit in.
  const int wellX = x + 4;
  const int wellY = y + 4;
  const int wellW = kWidth - 8;
  const int wellH = kHeight - 8;
  r.fillRect(wellX, wellY, wellW, wellH, true);

  const int keyW = (wellW - 6) / 2;
  const int keyH = wellH - 4;
  const int keyY = wellY + 2;
  const int leftX = wellX + 2;
  const int rightX = wellX + 2 + keyW + 2;
  constexpr int kWall = 4;    // visible height of a raised key's front face
  constexpr int kSink = 3;    // how far a pressed key sits below a raised one

  auto symbol = [&](const int cx, const int cy, const bool isOn) {
    if (isOn) {
      r.fillRect(cx - 1, cy - 5, 3, 10, true);
    } else {
      r.drawRect(cx - 4, cy - 5, 8, 10, 2, true);
    }
  };

  // Raised key: bright top face with a lit leading edge, and a dark wall
  // beneath it so it reads as standing proud of the well.
  auto raised = [&](const int kx, const bool isOn) {
    const int faceH = keyH - kWall;
    r.fillRect(kx, keyY, keyW, faceH, false);
    r.drawLine(kx, keyY, kx + keyW - 1, keyY);          // lit top edge
    r.fillRect(kx + keyW - 1, keyY, 1, faceH, true);    // shaded right edge
    r.fillRectDither(kx, keyY + faceH, keyW, kWall, Color::DarkGray);
    symbol(kx + keyW / 2, keyY + faceH / 2 + 1, isOn);
  };

  // Pressed key: sunk into the well with its top and leading edges shadowed.
  auto pressed = [&](const int kx, const bool isOn) {
    const int faceY = keyY + kSink;
    const int faceH = keyH - kSink;
    r.fillRectDither(kx, faceY, keyW, faceH, Color::LightGray);
    r.fillRect(kx, faceY, keyW, 2, true);   // shadow cast by the well's top
    r.fillRect(kx, faceY, 2, faceH, true);  // and its leading side
    // A small light plate keeps the symbol legible on the shaded face.
    const int cx = kx + 1 + keyW / 2;
    const int cy = faceY + 1 + faceH / 2;
    r.fillRect(cx - 6, cy - 6, 12, 12, false);
    symbol(cx, cy, isOn);
  };

  // On sits on the right: pressing the "I" key down (the switch is rocked to
  // the right) turns it on, leaving "O" standing proud on the left.
  if (on) {
    raised(leftX, false);
    pressed(rightX, true);
  } else {
    pressed(leftX, false);
    raised(rightX, true);
  }
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
