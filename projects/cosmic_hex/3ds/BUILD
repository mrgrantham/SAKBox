load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# The app's sources, shared by the app and its watching build
SOURCES = {
    "//projects/cosmic_hex/handheld:sources": "cosmic_hex/handheld",
    "//projects/cosmic_hex/rules:sources": "cosmic_hex/rules",
    "//projects/cosmic_hex/dice:sources": "cosmic_hex/dice",
    "//projects/cosmic_hex/models:rocket_3ds_sources": "assets",
    "//projects/scratchpadd/title_scene:sources": "scratchpadd/title_scene",
    "//projects/scratchpadd/memory_stats:sources": "scratchpadd/memory_stats",
    "//projects/scratchpadd/frame_arena:sources": "scratchpadd/frame_arena",
    "//projects/scratchpadd/motion:sources": "scratchpadd/motion",
    "//projects/cyclone:sources": "cyclone",
    "//projects/cosmic_hex/art:sources": "cosmic_hex/art",
    "//projects/scratchpadd/texture_recipe:sources": "scratchpadd/texture_recipe",
    # (IntegerDivide.hpp, which the recipe drawer uses)
    "//projects/flexmath:fixed_point_sources": "flexmath",
    "//projects/action_log:sources": "action_log",
    "//projects/game_bots:sources": "game_bots",
    "//projects/hex_grid:sources": "hex_grid",
    "//projects/seeded_random:sources": "seeded_random",
    "//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log",
    "//projects/scratchpadd/storage:sources": "scratchpadd/storage",
    "//projects/scratchpadd/network:sources": "scratchpadd/network",
    "//projects/scratchpadd/input:sources": "scratchpadd/input",
    "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
    "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
    "//projects/scratchpadd/app_ended:sources": "scratchpadd/app_ended",
    "//projects/scratchpadd/handheld_controls:sources": "scratchpadd/handheld_controls",
    "//projects/scratchpadd/scene:sources": "scratchpadd/scene",
    "//projects/scratchpadd/scene/3ds:sources": "scratchpadd/scene_3ds",
    "//projects/scratchpadd/scene/overlay_text:sources": "scratchpadd/overlay_text",
    "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
    "@stb//:stb_easy_font.h": "third_party/stb",
    "@spdlog//:sources": "third_party/spdlog",
}

# Cosmic Hex on a Nintendo 3DS (../handheld/source/cosmic_hex_on_a_handheld.cpp):
# the agreed rules, the bots, the board in 3D on the top screen. The first
# build (round 71): four bots playing, to watch. Needs Docker running (see
# //tools/devkitpro); `bazel run` opens it in Azahar, and tools/fbi_install
# installs it on a 3DS.
devkitpro_3ds_app(
    name = "cosmic-hex-3ds",
    tags = ["blender"],  # (the title screen's rocket: tools/assets)
    srcs = ["Makefile"],
    extra_srcs = SOURCES,
    icon_letters = "CH",
)

# The same app watching four bots by itself (COSMIC_HEX_WATCH_AT_START), for
# unattended runs in an emulator or on a console
devkitpro_3ds_app(
    name = "cosmic-hex-3ds-watch",
    tags = ["blender"],  # (the title screen's rocket: tools/assets)
    srcs = ["Makefile"],
    defines = ["COSMIC_HEX_WATCH_AT_START"],
    extra_srcs = SOURCES,
    icon_letters = "CW",
)
