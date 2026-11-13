// Each platform's thread priorities (ThreadPriority.hpp's table), read back
// with the platform's own call. Each is set in a thread of its own: on
// Linux a thread can lower its priority but not raise it again.
#include <ScratchPadd/ThreadPriority.hpp>
#include <gtest/gtest.h>

#include <string>
#include <thread>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace {

using ScratchPadd::Platform::ThreadPriority;

struct Result {
  bool set = false;
  std::string described;
};

Result setInANewThread(ThreadPriority priority) {
  Result result;
  std::thread([&] {
    result.set = ScratchPadd::Platform::setThisThreadPriority(priority);
    result.described = ScratchPadd::Platform::describeThisThreadPriority();
  }).join();
  return result;
}

} // namespace

TEST(ThreadPriorityTest, EachLevelIsThePlatformsOwn) {
  const Result high = setInANewThread(ThreadPriority::High);
  const Result normal = setInANewThread(ThreadPriority::Normal);
  const Result low = setInANewThread(ThreadPriority::Low);
  EXPECT_TRUE(normal.set);
  EXPECT_TRUE(low.set);
#if defined(__APPLE__)
  EXPECT_TRUE(high.set);
  EXPECT_EQ(high.described, "user-interactive");
  EXPECT_EQ(normal.described, "user-initiated");
  EXPECT_EQ(low.described, "utility");
#elif defined(_WIN32)
  EXPECT_TRUE(high.set);
  EXPECT_EQ(high.described, "above normal");
  EXPECT_EQ(normal.described, "normal");
  EXPECT_EQ(low.described, "below normal");
#elif defined(__linux__)
  // Raising a thread's priority needs privileges (root, or CAP_SYS_NICE):
  // without them High is refused, and says so, and the thread stays at 0
  EXPECT_EQ(high.described, high.set ? "nice -5" : "nice 0");
  if (geteuid() != 0) {
    EXPECT_FALSE(high.set);
  }
  EXPECT_EQ(normal.described, "nice 0");
  EXPECT_EQ(low.described, "nice 5");
#endif
}
