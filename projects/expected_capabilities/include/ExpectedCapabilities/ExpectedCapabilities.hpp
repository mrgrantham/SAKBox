#pragma once

// For tests that need something a machine may not have, such as a window,
// Vulkan or MIDI. Without it they skip, as there's nothing to test, unless
// the machine is meant to have it: SAKBOX_EXPECT, a comma-separated list
// (display,vulkan,midi, say), names what the checks' machines have, and
// there a test that would skip fails instead. A skip there means something
// is broken, such as the display not reaching the tests.
//
//   if (!display.isOpen())
//     SKIP_OR_FAIL_WITHOUT("display", "no window to draw in: " + display.error());

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <string_view>

namespace ExpectedCapabilities {

// Whether SAKBOX_EXPECT names the capability
inline bool expected(std::string_view capability) {
  const char *list = std::getenv("SAKBOX_EXPECT");
  if (!list)
    return false;
  std::string_view rest = list;
  while (!rest.empty()) {
    const auto comma = rest.find(',');
    if (rest.substr(0, comma) == capability)
      return true;
    rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
  }
  return false;
}

// Reports the capability missing: a skip, or a fatal failure if
// SAKBOX_EXPECT names it, saying why, at the caller's file and line. Use it
// through SKIP_OR_FAIL_WITHOUT, which returns after it.
inline void reportMissing(std::string_view capability, const std::string &why, const char *file,
                          int line) {
  const testing::ScopedTrace at(file, line, "needs " + std::string(capability));
  if (expected(capability))
    GTEST_FAIL() << why << " (and SAKBOX_EXPECT says this machine has " << capability << ")";
  GTEST_SKIP() << why;
}

} // namespace ExpectedCapabilities

// Skips the test, or fails it if SAKBOX_EXPECT names the capability, saying
// why. Like GTEST_SKIP, it returns from the function it's in, so it goes in
// the test or its SetUp.
#define SKIP_OR_FAIL_WITHOUT(capability, why)                                                      \
  return ::ExpectedCapabilities::reportMissing(capability, why, __FILE__, __LINE__)
