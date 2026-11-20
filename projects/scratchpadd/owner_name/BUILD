load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The name the console's owner gave it (OwnerName.hpp): a DS's nickname, a
# 3DS's user name; nothing elsewhere.
cc_library(
    name = "owner_name",
    srcs = ["src/OwnerName.cpp"],
    hdrs = ["include/ScratchPadd/OwnerName/OwnerName.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "OwnerName_test",
    size = "small",
    srcs = ["tests/OwnerName_test.cpp"],
    deps = [
        ":owner_name",
        "@gtest//:gtest_main",
    ],
)
