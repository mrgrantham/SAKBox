#pragma once

namespace ScratchPadd {

// While one of these exists, the system's timer runs at its finest (1 ms) on
// Windows, whose default (about 15.6 ms) rounds every sleep and timed wait up
// to it: a padd repeating every 16.7 ms (60 times a second) would wait two
// ticks, and fall behind. EventTimer holds one while its thread runs. Nothing
// elsewhere: other systems' timers are fine already.
class FineTimerResolution {
public:
  FineTimerResolution();
  ~FineTimerResolution(); // NOLINT(performance-trivially-destructible): not on Windows
  FineTimerResolution(const FineTimerResolution &) = delete;
  FineTimerResolution &operator=(const FineTimerResolution &) = delete;
};

} // namespace ScratchPadd
