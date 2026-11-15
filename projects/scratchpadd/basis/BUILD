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
    name = "MessageStats_test",
    size = "small",
    srcs = ["tests/MessageStats_test.cpp"],
    deps = [
        ":basis",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "AboutPadd_test",
    size = "small",
    srcs = ["tests/AboutPadd_test.cpp"],
    deps = [
        ":basis",
        "@gtest//:gtest_main",
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

cc_test(
  name = "SharedState_test",
  size = "small",
  srcs = ["tests/SharedState_test.cpp"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)

# Runs, with threads and without (SCRATCHPADD_NO_THREADS, as for some
# bare-metal targets): the same test built twice
cc_test(
  name = "Runs_test",
  size = "small",
  srcs = ["tests/Runs_test.cpp"],
  deps = ["@gtest//:gtest_main",
  ":basis"],
)

cc_test(
  name = "RunsUnthreaded_test",
  size = "small",
  srcs = ["tests/Runs_test.cpp"],
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

# What a message costs: round trips, bursts by payload size (and payloads
# that allocate), broadcasts to 1, 4 and 8 padds. bazel run -c opt (see
# the ScratchPadd README's message performance notes)
cc_binary(
    name = "message_benchmark",
    srcs = ["benchmarks/MessageBenchmark.cpp"],
    deps = [":basis"],
)

# How much having many padds costs on this device: the same work shared
# among 1 to 32 padds, how late a 2 ms repeat() comes with 0 to 32 padds
# busy (at the default priority and Urgently), and what idle padds cost to
# start, in memory and in CPU. bazel run -c opt (see the ScratchPadd
# README's padd count notes)
cc_binary(
    name = "padd_contention_benchmark",
    srcs = ["benchmarks/ContentionBenchmark.cpp"],
    linkopts = select({
        "@platforms//os:windows": ["psapi.lib"],
        "//conditions:default": [],
    }),
    deps = [":basis"],
)
