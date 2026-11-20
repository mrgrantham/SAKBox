load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The animation lab (desktop only): motions made with physics (Cyclone) in
# three ways (a solved throw, a guided spring, a search), flat or in 3D, and
# baked into ScratchPadd's BakedMotion for the consoles to play (Lab.hpp).
cc_library(
    name = "lab",
    srcs = ["src/Lab.cpp"],
    hdrs = ["include/AnimationLab/Lab.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/cyclone",
        "//projects/scratchpadd/motion",
        "//projects/seeded_random",
    ],
)

cc_test(
    name = "Lab_test",
    size = "small",
    srcs = ["tests/Lab_test.cpp"],
    deps = [
        ":lab",
        "@gtest//:gtest_main",
    ],
)
