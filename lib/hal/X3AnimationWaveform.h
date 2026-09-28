#pragma once

// A shorter Fast waveform for the Xteink X3's panel, used while a rapidly
// animated screen (the Marble Maze) is open. It is the stock Fast waveform with
// every phase's frame count halved, so each ball update is roughly twice as
// quick; blacks settle a little lighter, so it is only enabled on request.
//
// The panel driver in freeink-sdk takes its waveforms from a configuration the
// app can supply (see FREEINK_UC8253_X3_CONFIG in platformio.ini), which is how
// RetroInk swaps this in without changing the SDK. No effect on the X4.
namespace X3AnimationWaveform {

// Callers must not have a panel refresh in flight (HalDisplay waits for one).
void set(bool enabled);

}  // namespace X3AnimationWaveform
