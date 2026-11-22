load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Cosmic Hex as the consoles play it, without the console: the game at the
# table (people and bots, the dealer, the action log) and the board's 3D
# pieces. Built and tested on the Mac; the 3DS app (../3ds) builds these
# sources with its own (source/).
cc_library(
    name = "handheld",
    srcs = [
        "src/BoardButtons.cpp",
        "src/BoardMotion.cpp",
        "src/BoardPicture.cpp",
        "src/Hud.cpp",
        "src/GamePictures.cpp",
        "src/RecipePixels.cpp",
        "src/RollDice.cpp",
        "src/BoardPieces.cpp",
        "src/BottomScreen.cpp",
        "src/BuildMenu.cpp",
        "src/CardChoices.cpp",
        "src/CardPlaces.cpp",
        "src/CardsHeld.cpp",
        "src/ChoiceScreens.cpp",
        "src/ButtonMenu.cpp",
        "src/DicePieces.cpp",
        "src/Discovery.cpp",
        "src/MenuPicture.cpp",
        "src/MoveSpots.cpp",
        "src/OtherPlayers.cpp",
        "src/Pickers.cpp",
        "src/PlayerNames.cpp",
        "src/PointsBars.cpp",
        "src/ScreenFlow.cpp",
        "src/ScreenPictures.cpp",
        "src/ScorePops.cpp",
        "src/SettingsScreen.cpp",
        "src/StartMenu.cpp",
        "src/TableGame.cpp",
        "src/TopShows.cpp",
        "src/TopViews.cpp",
        "src/TradeArrival.cpp",
        "src/TradeRoom.cpp",
        "src/WinCelebration.cpp",
    ],
    hdrs = [
        "include/CosmicHex/Handheld/BoardButtons.hpp",
        "include/CosmicHex/Handheld/BoardMotion.hpp",
        "include/CosmicHex/Handheld/BoardPicture.hpp",
        "include/CosmicHex/Handheld/BoardPieces.hpp",
        "include/CosmicHex/Handheld/BottomScreen.hpp",
        "include/CosmicHex/Handheld/BoardView.hpp",
        "include/CosmicHex/Handheld/BuildMenu.hpp",
        "include/CosmicHex/Handheld/CardChoices.hpp",
        "include/CosmicHex/Handheld/CardPlaces.hpp",
        "include/CosmicHex/Handheld/CardsHeld.hpp",
        "include/CosmicHex/Handheld/ChoiceScreens.hpp",
        "include/CosmicHex/Handheld/ButtonMenu.hpp",
        "include/CosmicHex/Handheld/DicePieces.hpp",
        "include/CosmicHex/Handheld/GameWords.hpp",
        "include/CosmicHex/Handheld/Discovery.hpp",
        "include/CosmicHex/Handheld/MenuPicture.hpp",
        "include/CosmicHex/Handheld/MoveSpots.hpp",
        "include/CosmicHex/Handheld/Pickers.hpp",
        "include/CosmicHex/Handheld/OtherPlayers.hpp",
        "include/CosmicHex/Handheld/PlayerNames.hpp",
        "include/CosmicHex/Handheld/Palette.hpp",
        "include/CosmicHex/Handheld/PointsBars.hpp",
        "include/CosmicHex/Handheld/PlatformProfile.hpp",
        "include/CosmicHex/Handheld/Hud.hpp",
        "include/CosmicHex/Handheld/GamePictures.hpp",
        "include/CosmicHex/Handheld/RecipePixels.hpp",
        "include/CosmicHex/Handheld/RollDice.hpp",
        "include/CosmicHex/Handheld/ScreenFlow.hpp",
        "include/CosmicHex/Handheld/ScreenPictures.hpp",
        "include/CosmicHex/Handheld/ScorePops.hpp",
        "include/CosmicHex/Handheld/SettingsScreen.hpp",
        "include/CosmicHex/Handheld/StartMenu.hpp",
        "include/CosmicHex/Handheld/TableGame.hpp",
        "include/CosmicHex/Handheld/TopShows.hpp",
        "include/CosmicHex/Handheld/TopViews.hpp",
        "include/CosmicHex/Handheld/TradeArrival.hpp",
        "include/CosmicHex/Handheld/TradeRoom.hpp",
        "include/CosmicHex/Handheld/WinCelebration.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__", "//projects/animation_lab:__pkg__"],
    deps = [
        "//projects/scratchpadd/banner",
        "//projects/scratchpadd/buttons",
        "//projects/scratchpadd/on_screen_keyboard",
        "//projects/scratchpadd/picture_slot",
        "//projects/scratchpadd/still_frames",
        "//projects/scratchpadd/canvas",
        "//projects/scratchpadd/scene/overlay_text",
        "//projects/scratchpadd/motion",
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

# The app's parts that need no console (source/): the table's padd and its
# messages, the menus, the memory readout, the screens' sizes. Built on
# the Mac only so clang-tidy can check them (compile_commands.json); the
# consoles build every file in source/ with their Makefiles. ScreenPadd
# isn't here: it draws on a console's screens (its Display, ConsoleScreens'
# nds.h or 3ds.h) and the rocket the console build cooks with Blender.
cc_library(
    name = "app_parts",
    srcs = [
        "source/MemoryReadout.cpp",
        "source/MenuScreens.cpp",
        "source/TablePadd.cpp",
    ],
    hdrs = [
        "source/AppBasics.hpp",
        "source/MemoryReadout.hpp",
        "source/MenuScreens.hpp",
        "source/ScreenLayout.hpp",
        "source/TableMessages.hpp",
        "source/TablePadd.hpp",
    ],
    deps = [
        ":handheld",
        "//projects/cosmic_hex/rules",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/canvas",
        "//projects/scratchpadd/input",
        "//projects/scratchpadd/owner_name",
        "//projects/scratchpadd/storage",
        "@spdlog",
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

# The game's pictures loaded with the DS's texture memory emulated: needs a
# window (tagged display), as Renderer_test does
cc_test(
    name = "GamePictures_test",
    size = "small",
    srcs = ["tests/GamePictures_test.cpp"],
    tags = ["display"],
    deps = [
        ":handheld",
        "//projects/expected_capabilities",
        "//projects/scratchpadd/scene/desktop",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BoardPicture_test",
    size = "small",
    srcs = ["tests/BoardPicture_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/memory_stats:counting_allocator",  # (heap budgets)
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

cc_test(
    name = "BoardView_test",
    size = "small",
    srcs = ["tests/BoardView_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/scene/ds:display_list",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BuildMenu_test",
    size = "small",
    srcs = ["tests/BuildMenu_test.cpp"],
    deps = [
        ":handheld",
        "//projects/game_bots",
        "//projects/seeded_random",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "CardPlaces_test",
    size = "small",
    srcs = ["tests/CardPlaces_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/scene/desktop",  # (links a renderer; draws nothing)
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Hud_test",
    size = "small",
    srcs = ["tests/Hud_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/scene/desktop",  # (links a renderer; draws nothing)
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "PointsBars_test",
    size = "small",
    srcs = ["tests/PointsBars_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Pickers_test",
    size = "small",
    srcs = ["tests/Pickers_test.cpp"],
    deps = [
        ":handheld",
        "//projects/game_bots",
        "//projects/seeded_random",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "ChoiceScreens_test",
    size = "small",
    srcs = ["tests/ChoiceScreens_test.cpp"],
    deps = [
        ":handheld",
        "//projects/game_bots",
        "//projects/seeded_random",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BottomScreen_test",
    size = "small",
    srcs = ["tests/BottomScreen_test.cpp"],
    deps = [
        ":handheld",
        "//projects/game_bots",
        "//projects/seeded_random",
        "@gtest//:gtest_main",
    ],
)


cc_test(
    name = "ScreenPictures_test",
    size = "small",
    srcs = ["tests/ScreenPictures_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "CardsHeld_test",
    size = "small",
    srcs = ["tests/CardsHeld_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "OtherPlayers_test",
    size = "small",
    srcs = ["tests/OtherPlayers_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "TradeRoom_test",
    size = "small",
    srcs = ["tests/TradeRoom_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "TradeArrival_test",
    size = "small",
    srcs = ["tests/TradeArrival_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/scene/desktop",  # (links a renderer; draws nothing)
        "//projects/scratchpadd/scene/overlay_text",
        "//projects/scratchpadd/utilities",  # (GameClock: Animations Quick)
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Discovery_test",
    size = "small",
    srcs = ["tests/Discovery_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "PlayerNames_test",
    size = "small",
    srcs = ["tests/PlayerNames_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "TopShows_test",
    size = "small",
    srcs = ["tests/TopShows_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/utilities",  # (GameClock: Animations Quick)
        "@gtest//:gtest_main",
    ],
)

# The discovery's words and tile laid out on each handheld's screen in its
# font (no window needed): nothing overlaps
cc_test(
    name = "DiscoveryLayout_test",
    size = "small",
    srcs = ["tests/DiscoveryLayout_test.cpp"],
    deps = [
        ":handheld",
        "//projects/scratchpadd/scene/desktop",  # (links a renderer; draws nothing)
        "//projects/scratchpadd/scene/overlay_text",
        "@gtest//:gtest_main",
    ],
)

# The views over the board drawn through their timelines on a desktop's
# renderer (it needs a window): none may crash at any moment
cc_test(
    name = "TopViews_test",
    size = "small",
    srcs = ["tests/TopViews_test.cpp"],
    tags = ["display"],
    deps = [
        ":handheld",
        "//projects/expected_capabilities",
        "//projects/scratchpadd/scene/desktop",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "RollDice_test",
    size = "small",
    srcs = ["tests/RollDice_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "ScorePops_test",
    size = "small",
    srcs = ["tests/ScorePops_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "WinCelebration_test",
    size = "small",
    srcs = ["tests/WinCelebration_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "SettingsScreen_test",
    size = "small",
    srcs = ["tests/SettingsScreen_test.cpp"],
    deps = [
        ":handheld",
        "@gtest//:gtest_main",
    ],
)
