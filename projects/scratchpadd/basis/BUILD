load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

load("//tools/clang_format:clang_format.bzl", "clang_format")

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
        "@boost//:lockfree",
        "@boost//:program_options",
        "@boost//:filesystem",
        "@fmt",
        "@spdlog",
        "//projects/scratchpadd/utilities:utilities",
    ],
)


cc_test(
  name = "Test_Base",
  size = "small",
  srcs = ["tests/Test_Base.cpp"],
  deps = ["@gtest//:gtest_main",
  ":scratchpadd"],
)


cc_test(
  name = "Test_Threading",
  size = "small",
  srcs = ["tests/Test_Threading.cpp"],
  deps = ["@gtest//:gtest_main",
  ":scratchpadd"],
)

clang_format(name = "basis")
