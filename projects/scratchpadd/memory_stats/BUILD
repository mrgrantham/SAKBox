load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The counting allocator (round 89): the global new and delete replaced so
# MemoryStats (basis, ScratchPadd/MemoryStats.hpp) counts allocations, in
# all and by padd. An app opts in by depending on it; without it, nothing is
# counted (MemoryStats::linked() is false) and the heap figures still work.
cc_library(
    name = "counting_allocator",
    srcs = ["src/CountingAllocator.cpp"],
    alwayslink = True,  # (nothing names it: its new and delete replace the library's)
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/basis"],
)

cc_test(
    name = "MemoryStats_test",
    size = "small",
    srcs = ["tests/MemoryStats_test.cpp"],
    deps = [
        ":counting_allocator",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "ActivityPaddMemory_test",
    size = "small",
    srcs = ["tests/ActivityPaddMemory_test.cpp"],
    deps = [
        ":counting_allocator",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
    ],
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob(["src/**"]),
    visibility = ["//visibility:public"],
)
