load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Cosmic Hex's rules: plain C++, no screens or padds, described for the action
# log and the bots (see ../README.md and ../design.md)
cc_library(
    name = "rules",
    srcs = [
        "src/Describe.cpp",
        "src/Rules.cpp",
        "src/TradingRule.cpp",
    ],
    hdrs = [
        "include/CosmicHex/BotStyles.hpp",
        "include/CosmicHex/Describe.hpp",
        "include/CosmicHex/Rules.hpp",
        "include/CosmicHex/TradingRule.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        "//projects/action_log",
        "//projects/game_bots",
        "//projects/hex_grid",
        "//projects/seeded_random",
    ],
)

# The sources, for a 3DS or DS build (devkitPro's Makefile builds, given
# these by extra_srcs, with action_log's, game_bots', hex_grid's and
# seeded_random's; add rules/include to the Makefile's INCLUDES and rules/src
# to its SOURCES)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "Rules_test",
    srcs = ["tests/Rules_test.cpp"],
    deps = [
        ":rules",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Corners_test",
    srcs = ["tests/Corners_test.cpp"],
    deps = [
        ":rules",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "TradingRule_test",
    size = "small",
    srcs = ["tests/TradingRule_test.cpp"],
    deps = [
        ":rules",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "StartCards_test",
    size = "small",
    srcs = ["tests/StartCards_test.cpp"],
    deps = [
        ":rules",
        "//projects/game_bots",
        "//projects/seeded_random",
        "@gtest//:gtest_main",
    ],
)
