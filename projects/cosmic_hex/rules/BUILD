load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Cosmic Hex's rules: plain C++, no screens or padds, described for the action
# log and the bots (see ../README.md and ../design.md)
cc_library(
    name = "rules",
    srcs = [
        "src/Rules.cpp",
        "src/TradingRule.cpp",
    ],
    hdrs = [
        "include/CosmicHex/BotStyles.hpp",
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
