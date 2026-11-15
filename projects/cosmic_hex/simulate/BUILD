load("@rules_cc//cc:defs.bzl", "cc_binary")

# Plays many bot games and reports win rates by seat, game length and where
# the winners' points came from (see ../README.md#simulating)
cc_binary(
    name = "simulate",
    srcs = ["simulate.cpp"],
    deps = [
        "//projects/action_log",
        "//projects/cosmic_hex/rules",
        "//projects/game_bots",
        "//projects/seeded_random",
    ],
)
