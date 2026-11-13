#pragma once

// ThreadPriority.hpp - a thread's priority next to the main thread's, set
// with each platform's own call. Padds ask in their own words (Runs, in
// Threads.hpp) and Base sets their threads with this; EventTimer's threads
// take High, so a repeat() is queued on time.
//
//              High                Normal              Low
//   DS         main - 1            main + 1            main + 2      (calico: lower is higher)
//   3DS        main - 1            main + 1            main + 2      (libctru, the same)
//   macOS      user-interactive    user-initiated      utility       (quality-of-service classes)
//   Windows    above normal        normal              below normal
//   Linux      nice -5             nice 0              nice 5        (raising one needs privileges)
//
// "main" is the main thread's own priority on a console, read when
// System::start() calls rememberMainThread() (or the console's usual one,
// 0x1c on a DS and 0x30 on a 3DS, before that). On a console it decides
// what runs: a thread runs until it waits, and only a higher one can
// interrupt it. On a desktop it's a hint to a scheduler that shares several
// cores out anyway.

#include <string>

namespace ScratchPadd::Platform {

enum class ThreadPriority {
  High,   // ahead of the main thread
  Normal, // just behind it
  Low,    // behind Normal
};

// Remembers the calling thread as the main thread, whose priority the
// others are set next to on a console
void rememberMainThread();

// Sets the calling thread's priority. False where the platform refused (on
// Linux, raising a thread's priority needs privileges) or has none to set;
// the thread then keeps the one it had.
bool setThisThreadPriority(ThreadPriority priority);

// The calling thread's priority as the platform has it, for logs and tests
// ("priority 0x1d, main 0x1c", "utility", "nice 5")
std::string describeThisThreadPriority();

} // namespace ScratchPadd::Platform
