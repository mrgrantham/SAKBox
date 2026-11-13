#include <ScratchPadd/ThreadPriority.hpp>

#include <atomic>
#include <cstdio>

#if defined(__NDS__)
#include <nds.h>
#elif defined(__3DS__)
#include <3ds.h>
#elif defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace ScratchPadd::Platform {

namespace {

[[maybe_unused]] std::string hex(int value) {
  char text[16];
  std::snprintf(text, sizeof text, "0x%x", value);
  return text;
}

#if defined(__NDS__) || defined(__3DS__)
// A console's priorities: lower numbers run first
#if defined(__NDS__)
constexpr int kUsualMain = MAIN_THREAD_PRIO, kHighest = THREAD_MAX_PRIO, kLowest = THREAD_MIN_PRIO;
int thisThreadsPriority() { return threadGetSelf()->baseprio; }
bool setThisThreadsPriority(int priority) {
  threadSetPrio(threadGetSelf(), static_cast<u8>(priority));
  return true;
}
#else
constexpr int kUsualMain = 0x30, kHighest = 0x18, kLowest = 0x3f; // (an app's range)
int thisThreadsPriority() {
  s32 priority = kUsualMain;
  svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
  return priority;
}
bool setThisThreadsPriority(int priority) {
  return R_SUCCEEDED(svcSetThreadPriority(CUR_THREAD_HANDLE, priority));
}
#endif
std::atomic<int> mainPriority{kUsualMain};
#endif

} // namespace

void rememberMainThread() {
#if defined(__NDS__) || defined(__3DS__)
  mainPriority = thisThreadsPriority();
#endif
}

bool setThisThreadPriority(ThreadPriority priority) {
#if defined(__NDS__) || defined(__3DS__)
  const int offset = priority == ThreadPriority::High     ? -1
                     : priority == ThreadPriority::Normal ? 1
                                                          : 2;
  int wanted = mainPriority + offset;
  wanted = wanted < kHighest ? kHighest : wanted > kLowest ? kLowest : wanted;
  return setThisThreadsPriority(wanted);
#elif defined(__APPLE__)
  const qos_class_t qos = priority == ThreadPriority::High     ? QOS_CLASS_USER_INTERACTIVE
                          : priority == ThreadPriority::Normal ? QOS_CLASS_USER_INITIATED
                                                               : QOS_CLASS_UTILITY;
  return pthread_set_qos_class_self_np(qos, 0) == 0;
#elif defined(_WIN32)
  const int level = priority == ThreadPriority::High     ? THREAD_PRIORITY_ABOVE_NORMAL
                    : priority == ThreadPriority::Normal ? THREAD_PRIORITY_NORMAL
                                                         : THREAD_PRIORITY_BELOW_NORMAL;
  return SetThreadPriority(GetCurrentThread(), level) != 0;
#elif defined(__linux__)
  // (a thread's own nice value: Linux keeps one per thread)
  const int nice = priority == ThreadPriority::High     ? -5
                   : priority == ThreadPriority::Normal ? 0
                                                        : 5;
  return setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), nice) == 0;
#else
  (void)priority;
  return false;
#endif
}

std::string describeThisThreadPriority() {
#if defined(__NDS__) || defined(__3DS__)
  return "priority " + hex(thisThreadsPriority()) + ", main " + hex(mainPriority);
#elif defined(__APPLE__)
  switch (qos_class_self()) {
  case QOS_CLASS_USER_INTERACTIVE:
    return "user-interactive";
  case QOS_CLASS_USER_INITIATED:
    return "user-initiated";
  case QOS_CLASS_DEFAULT:
    return "default";
  case QOS_CLASS_UTILITY:
    return "utility";
  case QOS_CLASS_BACKGROUND:
    return "background";
  default:
    return "unspecified";
  }
#elif defined(_WIN32)
  switch (GetThreadPriority(GetCurrentThread())) {
  case THREAD_PRIORITY_ABOVE_NORMAL:
    return "above normal";
  case THREAD_PRIORITY_NORMAL:
    return "normal";
  case THREAD_PRIORITY_BELOW_NORMAL:
    return "below normal";
  default:
    return "priority " + std::to_string(GetThreadPriority(GetCurrentThread()));
  }
#elif defined(__linux__)
  return "nice " +
         std::to_string(getpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid))));
#else
  return "no thread priorities here";
#endif
}

} // namespace ScratchPadd::Platform
