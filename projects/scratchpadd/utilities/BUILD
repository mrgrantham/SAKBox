load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "utilities",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = ["src/Logger.cpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "@fmt",
        "@spdlog",
    ],
)

cc_test(
    name = "EventTimer_test",
    size = "small",
    srcs = ["tests/EventTimer_test.cpp"],
    deps = [
        ":utilities",
        "@gtest//:gtest_main",
    ],
)
