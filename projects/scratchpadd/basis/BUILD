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

# RunOn, with threads and without (SCRATCHPADD_NO_THREADS, as for a
# Nintendo DS): the same test built twice
cc_test(
  name = "RunOn_test",
  size = "small",
  srcs = ["tests/RunOn_test.cpp"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)

cc_test(
  name = "RunOnUnthreaded_test",
  size = "small",
  srcs = ["tests/RunOn_test.cpp"],
  local_defines = ["SCRATCHPADD_NO_THREADS"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)

# The sources, for builds outside Bazel's C++ rules (the 3DS and DS apps,
# staged into devkitPro's Makefiles; see //tools/devkitpro)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)
