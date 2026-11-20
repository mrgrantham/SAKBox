load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A texture kept for pictures that come and go (PictureSlot.hpp): a
# close-up drawn sharp when shown, into the same texture each time (the DS
# never gives texture memory back).
cc_library(
    name = "picture_slot",
    srcs = ["src/PictureSlot.cpp"],
    hdrs = ["include/ScratchPadd/PictureSlot/PictureSlot.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/scene"],
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

# (on a desktop's renderer: it needs a real one)
cc_test(
    name = "PictureSlot_test",
    size = "small",
    srcs = ["tests/PictureSlot_test.cpp"],
    tags = ["display"],
    deps = [
        ":picture_slot",
        "//projects/expected_capabilities",
        "//projects/scratchpadd/scene/desktop",
        "@gtest//:gtest_main",
    ],
)
