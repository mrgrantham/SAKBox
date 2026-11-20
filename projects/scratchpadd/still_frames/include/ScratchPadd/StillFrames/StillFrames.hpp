// StillFrames.hpp - when a screen can skip drawing and show its last frame
// again: a DS's 3D hardware redraws what it was last sent by itself, so an
// app that sends nothing while nothing changes leaves the CPU to its other
// work (Cosmic Hex's bots went from 5 games a move to 84). The rule:
// draw while anything moves or changes, and one frame more after the last
// move, so the last frame drawn shows where things stopped (a banner's last
// frame still had a trace of its fading band: the user saw it stay).
//
//   StillFrames still;
//   if (still.canHold(moving, changed)) { wait for the refresh; }
//   else { draw(); still.drawn(moving); }
#pragma once

namespace ScratchPadd {

class StillFrames {
public:
  // Whether the last frame can be shown again: nothing moves now, nothing
  // changed since it was drawn, and nothing moved when it was drawn
  bool canHold(bool moving, bool changed) const { return !moving && !changed && !movedWhenDrawn_; }
  // A frame drawn, while something moved or not
  void drawn(bool moving) { movedWhenDrawn_ = moving; }

private:
  bool movedWhenDrawn_ = true; // (nothing drawn yet: draw)
};

} // namespace ScratchPadd
