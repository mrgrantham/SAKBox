load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Cosmic Hex as the consoles play it, without the console: the game at the
# table (people and bots, the dealer, the action log) and the board's 3D
# pieces. Built and tested on the Mac; the 3DS app (../3ds) builds these
# sources with its own (source/).
cc_library(
    name = "handheld",
    srcs = [
        "src/BoardMotion.cpp",
        "src/BoardPicture.cpp",
        "src/BoardPieces.cpp",
        "src/MenuPicture.cpp",
        "src/MoveSpots.cpp",
        "src/StartMenu.cpp",
        "src/TableGame.cpp",
    ],
    hdrs = [
        "include/CosmicHex/Handheld/BoardMotion.hpp",
        "include/CosmicHex/Handheld/BoardPicture.hpp",
        "include/CosmicHex/Handheld/BoardPieces.hpp",
        "include/CosmicHex/Handheld/MenuPicture.hpp",
        "include/CosmicHex/Handheld/MoveSpots.hpp",
        "include/CosmicHex/Handheld/StartMenu.hpp",
        "include/CosmicHex/Handheld/TableGame.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        "//projects/action_log",
        "//projects/cosmic_hex/art",
        "//projects/cosmic_hex/rules",
        "//projects/game_bots",
        "//projects/hex_grid",
        "//projects/scratchpadd/scene",
        "@stb",
        "//projects/scratchpadd/texture_recipe",
        "//projects/seeded_random",
    ],
)

# The sources, for the consoles' Makefile builds (the app's own in source/)
filegroup(
    name = "sources",
    srcs = glob([
        "include/**",
        "src/**",
        "source/**",
    ]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "TableGame_test",
    size = "medium",
    srcs = ["tests/TableGame_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BoardPieces_test",
    size = "small",
    srcs = ["tests/BoardPieces_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BoardMotion_test",
    size = "small",
    srcs = ["tests/BoardMotion_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BoardPicture_test",
    size = "small",
    srcs = ["tests/BoardPicture_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "MoveSpots_test",
    size = "small",
    srcs = ["tests/MoveSpots_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "StartMenu_test",
    size = "small",
    srcs = ["tests/StartMenu_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)
