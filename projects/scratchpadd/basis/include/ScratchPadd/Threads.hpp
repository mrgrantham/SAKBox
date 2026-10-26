#pragma once

// Whether this platform has threads, and the choice each padd makes of which
// thread to run on (RunOn, the answer to Base::runOn()).
//
// SCRATCHPADD_HAS_THREADS is 1 where the C++ library has threads and 0 where
// it doesn't (a C++ library built without them, as for some bare-metal
// targets). It's worked out at compile time from the C++ library's own
// configuration (libstdc++ defines _GLIBCXX_HAS_GTHREADS when it has threads;
// libc++ sets _LIBCPP_HAS_THREADS, or before version 20 defines
// _LIBCPP_HAS_NO_THREADS when it hasn't), and it's 0 wherever
// SCRATCHPADD_NO_THREADS is defined (RunOnUnthreaded_test does, to check a
// build without threads on a desktop). The 3DS and DS have threads: libctru's,
// and calico's (devkitPro's DS system library) behind std::thread.

#include <version> // the C++ library's configuration macros

#if defined(SCRATCHPADD_NO_THREADS)
#define SCRATCHPADD_HAS_THREADS 0
#elif defined(__GLIBCXX__) && !defined(_GLIBCXX_HAS_GTHREADS)
#define SCRATCHPADD_HAS_THREADS 0
#elif defined(_LIBCPP_VERSION) && defined(_LIBCPP_HAS_THREADS) && !_LIBCPP_HAS_THREADS
#define SCRATCHPADD_HAS_THREADS 0
#elif defined(_LIBCPP_VERSION) && defined(_LIBCPP_HAS_NO_THREADS)
#define SCRATCHPADD_HAS_THREADS 0
#else
#define SCRATCHPADD_HAS_THREADS 1
#endif

namespace ScratchPadd {

inline constexpr bool kHasThreads = SCRATCHPADD_HAS_THREADS;

// Which thread a padd's work runs on: what a padd's runOn() returns.
enum class RunOn {
  // The thread that called System::start(), in one loop shared with the other
  // main-thread padds (see the README's Threads section). For work that must
  // be on the main thread, such as windowing on macOS.
  MainThread,
#if SCRATCHPADD_HAS_THREADS
  // A thread of the padd's own. Only exists where there are threads: without
  // them, a padd that returns RunOn::BackgroundThread doesn't compile, so a
  // padd can't ask for a thread the platform can't give it.
  BackgroundThread,
#endif
};

// What runOn() returns unless a padd overrides it: a background thread where
// there are threads, the main thread where there aren't
inline constexpr RunOn kDefaultRunOn =
#if SCRATCHPADD_HAS_THREADS
    RunOn::BackgroundThread;
#else
    RunOn::MainThread;
#endif

} // namespace ScratchPadd
