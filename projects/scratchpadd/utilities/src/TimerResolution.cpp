#include <ScratchPadd/TimerResolution.hpp>

// (windows.h kept to this file: its macros, min, max, small and the rest,
// clash with ordinary names)
#if defined(_WIN32)
// (first, in a block of its own so clang-format keeps it first: timeapi.h
// uses its types)
#include <windows.h>

#include <timeapi.h>
#endif

namespace ScratchPadd {

#if defined(_WIN32)
FineTimerResolution::FineTimerResolution() { timeBeginPeriod(1); }
FineTimerResolution::~FineTimerResolution() { timeEndPeriod(1); }
#else
FineTimerResolution::FineTimerResolution() = default;
FineTimerResolution::~FineTimerResolution() = default;
#endif

} // namespace ScratchPadd
