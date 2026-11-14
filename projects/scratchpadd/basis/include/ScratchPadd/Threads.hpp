#pragma once

// Whether this platform has threads, and how each padd's work runs (Runs,
// the answer to Base::runs()).
//
// SCRATCHPADD_HAS_THREADS is 1 where the C++ library has threads and 0 where
// it doesn't (a C++ library built without them, as for some bare-metal
// targets). It's worked out at compile time from the C++ library's own
// configuration (libstdc++ defines _GLIBCXX_HAS_GTHREADS when it has threads;
// libc++ sets _LIBCPP_HAS_THREADS, or before version 20 defines
// _LIBCPP_HAS_NO_THREADS when it hasn't), and it's 0 wherever
// SCRATCHPADD_NO_THREADS is defined (RunsUnthreaded_test does, to check a
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

// How a padd's work runs: what a padd's runs() returns. Either in the main
// thread's loop, or on a thread of the padd's own, at a priority next to
// the main thread's: when several have work, the more urgent goes first.
// (On a DS or 3DS that's strict: a thread runs until it waits, and only a
// more urgent one can interrupt it. A desktop's scheduler shares its cores
// out anyway, so there it matters only when they're busy.)
enum class Runs {
  // In the thread that called System::start(), in one loop shared with the
  // other main-thread padds (see the README's Threads section), at that
  // thread's priority. For work that must be on the main thread, such as
  // windowing on macOS, and the screens and buttons of a console.
  OnMainThread,
#if SCRATCHPADD_HAS_THREADS
  // The rest are a thread of the padd's own. They only exist where there are
  // threads: without them, a padd that returns one doesn't compile, so a
  // padd can't ask for a thread the platform can't give it.
  //
  // Ahead of the main thread: short, time-critical work (audio)
  Urgently,
  // Just behind the main thread, so the screens and buttons always come
  // first (the default)
  Steadily,
  // Only when nothing else has work (writing a log to a card)
  WhenIdle,
#endif
};

// What runs() returns unless a padd overrides it: a thread of its own where
// there are threads, the main thread where there aren't
inline constexpr Runs kDefaultRuns =
#if SCRATCHPADD_HAS_THREADS
    Runs::Steadily;
#else
    Runs::OnMainThread;
#endif

// The enumerator's name ("Steadily"), for tables
inline const char *nameOf(Runs runs) {
  switch (runs) {
  case Runs::OnMainThread:
    return "OnMainThread";
#if SCRATCHPADD_HAS_THREADS
  case Runs::Urgently:
    return "Urgently";
  case Runs::Steadily:
    return "Steadily";
  case Runs::WhenIdle:
    return "WhenIdle";
#endif
  }
  return "?";
}

// How a padd runs, in words, for logs ("steadily")
inline const char *describe(Runs runs) {
  switch (runs) {
  case Runs::OnMainThread:
    return "on the main thread";
#if SCRATCHPADD_HAS_THREADS
  case Runs::Urgently:
    return "urgently";
  case Runs::Steadily:
    return "steadily";
  case Runs::WhenIdle:
    return "when idle";
#endif
  }
  return "?";
}

} // namespace ScratchPadd
