load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Cyclone's ragdoll on a DS or 3DS, in padds: one app's source, which the
# DS's and the 3DS's builds (../ds, ../3ds) each compile in their Makefile
# builds (as `ragdoll`, in their SOURCES)
filegroup(
    name = "sources",
    srcs = glob(["source/**"]),
    visibility = [
        "//projects/cyclone/3ds:__pkg__",
        "//projects/cyclone/ds:__pkg__",
    ],
)

# The sweep's bookkeeping (R in the app: every setting run in turn and
# ranked), plain C++, tested here
cc_library(
    name = "physics_sweep",
    hdrs = ["source/physics_sweep.h"],
    strip_include_prefix = "source",
    visibility = ["//projects/cyclone:__pkg__"],  # (ragdoll_sweep, on desktops)
)

cc_test(
    name = "physics_sweep_test",
    size = "small",
    srcs = ["tests/physics_sweep_test.cpp"],
    deps = [
        ":physics_sweep",
        "@gtest//:gtest_main",
    ],
)
