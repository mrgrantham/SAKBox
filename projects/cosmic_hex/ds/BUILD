load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

# The app's sources, shared by the app and its watching build
SOURCES = {
    "//projects/cosmic_hex/handheld:sources": "cosmic_hex/handheld",
    "//projects/cosmic_hex/rules:sources": "cosmic_hex/rules",
    "//projects/cosmic_hex/dice:sources": "cosmic_hex/dice",
    "//projects/cosmic_hex/models:rocket_ds_sources": "assets",
    "//projects/scratchpadd/title_scene:sources": "scratchpadd/title_scene",
    "//projects/scratchpadd/memory_stats:sources": "scratchpadd/memory_stats",
    "//projects/scratchpadd/frame_arena:sources": "scratchpadd/frame_arena",
    "//projects/scratchpadd/motion:sources": "scratchpadd/motion",
    "//projects/scratchpadd/canvas:sources": "scratchpadd/canvas",
    "//projects/scratchpadd/startup_splash:sources": "scratchpadd/startup_splash",
    "//projects/scratchpadd/console_screens:sources": "scratchpadd/console_screens",
    "//projects/cyclone:sources": "cyclone",
    "//projects/cosmic_hex/art:sources": "cosmic_hex/art",
    "//projects/scratchpadd/texture_recipe:sources": "scratchpadd/texture_recipe",
    "//projects/action_log:sources": "action_log",
    "//projects/game_bots:sources": "game_bots",
    "//projects/hex_grid:sources": "hex_grid",
    "//projects/seeded_random:sources": "seeded_random",
    "//projects/flexmath:fixed_point_sources": "flexmath",
    "//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log",
    "//projects/scratchpadd/storage:sources": "scratchpadd/storage",
    "//projects/scratchpadd/input:sources": "scratchpadd/input",
    "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
    "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
    "//projects/scratchpadd/app_ended:sources": "scratchpadd/app_ended",
    "//projects/scratchpadd/handheld_controls:sources": "scratchpadd/handheld_controls",
    "//projects/scratchpadd/scene:sources": "scratchpadd/scene",
    "//projects/scratchpadd/scene/ds:sources": "scratchpadd/scene_ds",
    "//projects/scratchpadd/scene/overlay_text:sources": "scratchpadd/overlay_text",
    "@stb//:stb_easy_font.h": "third_party/stb",
    "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
    "//projects/scratchpadd/ds/platform:sources": "scratchpadd_ds/platform",
    "@spdlog//:sources": "third_party/spdlog",
}

# Cosmic Hex on a Nintendo DS (../handheld/source/cosmic_hex_on_a_handheld.cpp,
# as the 3DS's ../3ds): the agreed rules, the bots thinking less, the board in
# 3D on the top screen (tiles in their face's colour: the scene layer's DS
# backend has no textures yet) and drawn flat on the bottom, to touch. Needs
# Docker running (see //tools/devkitpro); `bazel run` opens it in melonDS.
devkitpro_nds_app(
    name = "cosmic-hex-ds",
    tags = ["blender"],  # (the title screen's rocket: tools/assets)
    srcs = ["Makefile"],
    extra_srcs = SOURCES,
)

# The bots' parts timed on their own (COSMIC_HEX_BENCH: CosmicHexBench.hpp),
# a table on the screen to compare melonDS with a DS (the
# emulator showed the bots about five times as fast as a DS)
devkitpro_nds_app(
    name = "cosmic-hex-ds-bench",
    tags = ["blender"],  # (the title screen's rocket: tools/assets)
    srcs = ["Makefile"],
    defines = ["COSMIC_HEX_BENCH"],
    extra_srcs = SOURCES,
    icon_letters = "CB",
)

# The same app watching four bots by itself (COSMIC_HEX_WATCH_AT_START), for
# unattended runs in an emulator or on a console
devkitpro_nds_app(
    name = "cosmic-hex-ds-watch",
    tags = ["blender"],  # (the title screen's rocket: tools/assets)
    srcs = ["Makefile"],
    defines = ["COSMIC_HEX_WATCH_AT_START"],
    extra_srcs = SOURCES,
    icon_letters = "CW",
)

# The watching build with scripted taps (COSMIC_HEX_SCRIPTED_TAPS): the app
# taps its own bottom screen at set times (Show resources, open and closed),
# so screens reached by touch can be seen in melonDS
devkitpro_nds_app(
    name = "cosmic-hex-ds-taps",
    tags = ["blender"],  # (the title screen's rocket: tools/assets)
    srcs = ["Makefile"],
    defines = [
        "COSMIC_HEX_WATCH_AT_START",
        "COSMIC_HEX_SCRIPTED_TAPS",
    ],
    extra_srcs = SOURCES,
    icon_letters = "CT",
)
