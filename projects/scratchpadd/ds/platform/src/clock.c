// std::chrono's clocks on the Nintendo DS.
//
// newlib's clock_gettime() asks the platform through a hook,
// __syscall_clock_gettime, which calico (devkitPro's DS system library)
// doesn't provide. Without it, clock_gettime() fills in nothing, so
// std::chrono::steady_clock read garbage, and every timed wait (a padd's
// repeat() timer, std::condition_variable::wait_for) went wrong. This
// provides it from calico's system tick counter (TICK_FREQ, about 523 kHz:
// a tick is about 1.9 us).
//
// CLOCK_MONOTONIC counts ticks since the DS started. CLOCK_REALTIME is the
// DS's real-time clock as read the first time it's asked for, carried
// forward by the same ticks. The real-time clock itself only counts whole
// seconds, and that matters: newlib times every condition-variable wait
// against CLOCK_REALTIME (libstdc++ turns a steady_clock deadline into one
// on that clock), so with whole seconds a 200 ms wait returned at once.
//
// calico starts the tick counter the first time something is scheduled on
// it (a sleep, a timer); until then it reads 0. So each reading starts it
// first (tickInit() does nothing once it's running).

#include <calico/system/tick.h>

#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

int __syscall_clock_gettime(clockid_t clock, struct timespec *time) {
  tickInit();
  if (clock == CLOCK_MONOTONIC) {
    const uint64_t ticks = tickGetCount();
    time->tv_sec = (time_t)(ticks / TICK_FREQ);
    time->tv_nsec = (long)((ticks % TICK_FREQ) * 1000000000ull / TICK_FREQ);
    return 0;
  }
  if (clock == CLOCK_REALTIME) {
    // The real-time clock's reading, and the tick count, when first asked
    static bool started = false;
    static time_t startSeconds;
    static uint64_t startTicks;
    if (!started) {
      struct timeval now;
      if (gettimeofday(&now, NULL) != 0) {
        return -1;
      }
      startSeconds = now.tv_sec;
      startTicks = tickGetCount();
      started = true;
    }
    const uint64_t ticks = tickGetCount() - startTicks;
    time->tv_sec = startSeconds + (time_t)(ticks / TICK_FREQ);
    time->tv_nsec = (long)((ticks % TICK_FREQ) * 1000000000ull / TICK_FREQ);
    return 0;
  }
  errno = EINVAL;
  return -1;
}

int __syscall_clock_getres(clockid_t clock, struct timespec *resolution) {
  if (clock != CLOCK_MONOTONIC && clock != CLOCK_REALTIME) {
    errno = EINVAL;
    return -1;
  }
  resolution->tv_sec = 0;
  resolution->tv_nsec = (long)((1000000000ull + TICK_FREQ - 1) / TICK_FREQ); // a tick
  return 0;
}
