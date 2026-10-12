#include "RockerSwitch.h"

#include <GfxRenderer.h>

#include <algorithm>

#include "fontIds.h"

namespace {
constexpr int kKnob = RockerSwitch::kHeight - 2 * RockerSwitch::kKnobInset;

struct Pending {
  bool valid = false;
  int x = 0;
  int y = 0;
};
Pending pending;

void drawCell(const GfxRenderer& r, const int x, const int y, const int w, const int h, const char* letter,
              const bool pressed) {
  r.fillRect(x, y, w, h, pressed);
  if (!pressed) {
    // Raised half: a thick lower/right edge like a physical rocker's lip.
    r.drawLine(x, y + h - 1, x + w - 1, y + h - 1);
    r.drawLine(x, y + h - 2, x + w - 1, y + h - 2);
  }
  constexpr int font = UI_10_FONT_ID;
  const int textW = r.getTextWidth(font, letter);
  const int textX = x + std::max(0, (w - textW) / 2);
  const int textY = y + std::max(0, (h - r.getLineHeight(font)) / 2) - (pressed ? 0 : 1);
  r.drawText(font, textX, textY, letter, !pressed);
}

void paintRocker(const GfxRenderer& r, const int x, const int y, const bool on) {
  // Opaque backing so a selected (inverted) row can't change how it reads.
  r.fillRect(x - 2, y - 2, RockerSwitch::kWidth + 4, RockerSwitch::kHeight + 4, false);
  r.drawRect(x, y, RockerSwitch::kWidth, RockerSwitch::kHeight);
  r.drawRect(x + 1, y + 1, RockerSwitch::kWidth - 2, RockerSwitch::kHeight - 2);
  const int innerX = x + 2;
  const int innerY = y + 2;
  const int innerH = RockerSwitch::kHeight - 4;
  const int half = (RockerSwitch::kWidth - 4) / 2;
  drawCell(r, innerX, innerY, half, innerH, "O", !on);
  drawCell(r, innerX + half, innerY, RockerSwitch::kWidth - 4 - half, innerH, "I", on);
}

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
  paintRocker(r, track.x, track.y, knobOffset > RockerSwitch::kWidth / 2 - kKnob / 2);
}
}  // namespace

namespace RockerSwitch {
void setEnabled(const bool enabled) {
  pending.valid = false;
  GfxRenderer::setFillObserver(enabled ? &onFill : nullptr);
}
}  // namespace RockerSwitch
