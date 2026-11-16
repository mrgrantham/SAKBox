load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# Bot games played one at a time (Batch.hpp): the simulator's and the
# balance lab's
cc_library(
    name = "batch",
    srcs = ["Batch.cpp"],
    hdrs = ["Batch.hpp"],
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        "//projects/action_log",
        "//projects/cosmic_hex/rules",
        "//projects/game_bots",
        "//projects/seeded_random",
    ],
)

# Plays many bot games and reports win rates by seat, game length and where
# the winners' points came from (see ../README.md#simulating)
cc_binary(
    name = "simulate",
    srcs = ["simulate.cpp"],
    deps = [
        ":batch",
        "//projects/action_log",
        "//projects/cosmic_hex/rules",
        "//projects/game_bots",
        "//projects/seeded_random",
    ],
)
