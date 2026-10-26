load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")


cc_library(
    name = "basis",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = [],
    visibility = ["//visibility:public"],
    defines = [
        "PROJECT_NAME=\\\"SCRATCHPADD\\\"",
    ],
    strip_include_prefix = "include/",
    deps = [
        "@fmt",
        "@spdlog",
        "//projects/scratchpadd/messages",
        "//projects/scratchpadd/utilities:utilities",
    ],
)


cc_test(
  name = "Base_test",
  size = "small",
  srcs = ["tests/Base_test.cpp"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)


cc_test(
  name = "Examples_test",
  size = "small",
  srcs = ["tests/Examples_test.cpp"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)


cc_test(
  name = "Threading_test",
  size = "small",
  srcs = ["tests/Threading_test.cpp"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)
