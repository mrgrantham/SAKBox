load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/lto:lto.bzl", "LTO_COPTS", "LTO_LINKOPTS")

# Bot games played one at a time (Batch.hpp): the simulator's and the
# balance lab's
cc_library(
    name = "batch",
    copts = LTO_COPTS,
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
    copts = LTO_COPTS,
    linkopts = LTO_LINKOPTS,
    deps = [
        ":batch",
        # (new and delete through it, so --arena takes effect)
        "//projects/scratchpadd/memory_stats:counting_allocator",
        "//projects/action_log",
        "//projects/cosmic_hex/rules",
        "//projects/game_bots",
        "//projects/seeded_random",
    ],
)
