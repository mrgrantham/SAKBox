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
        "src/RecipePixels.cpp",
        "src/BoardPieces.cpp",
        "src/ButtonMenu.cpp",
        "src/DicePieces.cpp",
        "src/MenuPicture.cpp",
        "src/MoveSpots.cpp",
        "src/ScreenFlow.cpp",
        "src/StartMenu.cpp",
        "src/TableGame.cpp",
    ],
    hdrs = [
        "include/CosmicHex/Handheld/BoardMotion.hpp",
        "include/CosmicHex/Handheld/BoardPicture.hpp",
        "include/CosmicHex/Handheld/BoardPieces.hpp",
        "include/CosmicHex/Handheld/ButtonMenu.hpp",
        "include/CosmicHex/Handheld/DicePieces.hpp",
        "include/CosmicHex/Handheld/MenuPicture.hpp",
        "include/CosmicHex/Handheld/MoveSpots.hpp",
        "include/CosmicHex/Handheld/PlatformProfile.hpp",
        "include/CosmicHex/Handheld/RecipePixels.hpp",
        "include/CosmicHex/Handheld/ScreenFlow.hpp",
        "include/CosmicHex/Handheld/StartMenu.hpp",
        "include/CosmicHex/Handheld/TableGame.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        "//projects/action_log",
        "//projects/cosmic_hex/art",
        "//projects/cosmic_hex/dice",
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

cc_test(
    name = "DicePieces_test",
    size = "small",
    srcs = ["tests/DicePieces_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "ScreenFlow_test",
    size = "small",
    srcs = ["tests/ScreenFlow_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "TableGameArena_test",
    size = "medium",
    srcs = ["tests/TableGameArena_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/memory_stats:counting_allocator",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "PlatformProfile_test",
    size = "small",
    srcs = ["tests/PlatformProfile_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)
