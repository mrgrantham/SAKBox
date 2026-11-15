load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Hex grid maths: coordinates, neighbours, distances, rings, lines, rotation,
# a hexagon-shaped board, paths, and where hexes are on screen (see
# README.md). Header-only, with no dependencies, so the consoles' Makefile
# builds take it as it is.
cc_library(
    name = "hex_grid",
    hdrs = [
        "include/HexGrid/Hex.hpp",
        "include/HexGrid/HexagonMap.hpp",
        "include/HexGrid/Layout.hpp",
        "include/HexGrid/Paths.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The headers, for a DS or 3DS build (devkitPro's Makefile builds, given
# these by extra_srcs; add hex_grid/include to the Makefile's INCLUDES)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)

[cc_test(
    name = test,
    srcs = ["tests/" + test + ".cpp"],
    deps = [
        ":hex_grid",
        "@gtest//:gtest_main",
    ],
) for test in [
    "Hex_test",
    "HexagonMap_test",
    "Layout_test",
    "Paths_test",
]]
