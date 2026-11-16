load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Cosmic Hex's rules: plain C++, no screens or padds, described for the action
# log and the bots (see ../README.md and ../design.md)
cc_library(
    name = "rules",
    srcs = ["src/Rules.cpp"],
    hdrs = [
        "include/CosmicHex/BotStyles.hpp",
        "include/CosmicHex/Rules.hpp",
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
