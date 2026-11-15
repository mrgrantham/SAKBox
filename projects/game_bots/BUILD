load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Bot players for any game described as a GameBots::BotGame: a random bot
# and ISMCTS, plus playMatch for simulated games recorded in action logs
# (see README.md). Header-only.
cc_library(
    name = "game_bots",
    hdrs = [
        "include/GameBots/BotGame.hpp",
        "include/GameBots/Bots.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/action_log",
        "//projects/seeded_random",
    ],
)

# The headers, for a DS or 3DS build (devkitPro's Makefile builds, given
# these by extra_srcs, with action_log's and seeded_random's; add
# game_bots/include to the Makefile's INCLUDES)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "Bots_test",
    srcs = [
        "tests/Bots_test.cpp",
        "tests/TestGames.hpp",
    ],
    deps = [
        ":game_bots",
        "@gtest//:gtest_main",
    ],
)
