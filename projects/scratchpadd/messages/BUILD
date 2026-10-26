load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

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
  name = "Controls_test",
  size = "small",
  srcs = ["tests/Controls_test.cpp"],
  deps = ["@gtest//:gtest_main",
  "//projects/scratchpadd/basis:basis",
  "//projects/data_dep_retriever",
  ":messages"],
)

cc_test(
  name = "Messages_test",
  size = "small",
  srcs = ["tests/Messages_test.cpp"],
  deps = ["@gtest//:gtest_main",
  "//projects/scratchpadd/basis:basis",
  ":messages",
  ],
)

# The sources, for builds outside Bazel's C++ rules (the 3DS and DS apps,
# staged into devkitPro's Makefiles; see //tools/devkitpro)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)
