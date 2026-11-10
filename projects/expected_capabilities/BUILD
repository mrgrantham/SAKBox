load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# For tests that need something a machine may not have (a window, Vulkan,
# MIDI): they skip without it, or fail where SAKBOX_EXPECT says it's there
cc_library(
    name = "expected_capabilities",
    testonly = True,
    hdrs = ["include/ExpectedCapabilities/ExpectedCapabilities.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["@gtest"],
)

cc_test(
    name = "ExpectedCapabilities_test",
    size = "small",
    srcs = ["tests/ExpectedCapabilities_test.cpp"],
    deps = [
        ":expected_capabilities",
        "@gtest//:gtest_main",
    ],
)
