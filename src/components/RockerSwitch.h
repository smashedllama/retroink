#pragma once

class GfxRenderer;

// System 6 draws on/off switches as a two-position rocker: "O" and "I" side by
// side with the active half pressed in. FreeInkUI only knows a sliding switch,
// so the SDK is given a switch of this exact (unusual) size and the rocker is
// painted over it as it is drawn.
namespace RockerSwitch {
constexpr int kWidth = 47;
constexpr int kHeight = 27;
constexpr int kKnobInset = 3;

// Starts or stops repainting SDK switches; follows the active UI theme.
void setEnabled(bool enabled);
}  // namespace RockerSwitch
