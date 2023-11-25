load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "messages",
    hdrs = glob(["include/**/*.hpp"]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "include/",
    deps = [
        "@fmt",
        "@spdlog",
    ],
)


cc_test(
  name = "Test_Controls",
  size = "small",
  srcs = ["tests/Test_Controls.cpp"],
  deps = ["@gtest//:gtest_main",
  "//projects/scratchpadd/basis:basis",
  ":messages"],
)

cc_test(
  name = "Test_Messages",
  size = "small",
  srcs = ["tests/Test_Messages.cpp"],
  deps = ["@gtest//:gtest_main",
  "//projects/scratchpadd/basis:basis",
  ":messages",
  ],
)