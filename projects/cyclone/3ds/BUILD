load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# The app's sources, shared by the ragdoll and its sweep
RAGDOLL_SOURCES = {
    "//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log",
    "//projects/scratchpadd/storage:sources": "scratchpadd/storage",
    "//projects/scratchpadd/input:sources": "scratchpadd/input",
    "//projects/cyclone/handheld:sources": "ragdoll",
    "//projects/cyclone:sources": "cyclone",
    "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
    "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
    "//projects/scratchpadd/app_ended:sources": "scratchpadd/app_ended",
    "//projects/scratchpadd/handheld_controls:sources": "scratchpadd/handheld_controls",
    "//projects/scratchpadd/scene:sources": "scratchpadd/scene",
    "//projects/scratchpadd/scene/3ds:sources": "scratchpadd/scene_3ds",
    "//projects/scratchpadd/scene/overlay_text:sources": "scratchpadd/overlay_text",
    "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
    "//projects/scratchpadd/audio/made_sounds_pcm16:sources": "scratchpadd_audio_pcm16",
    "//projects/scratchpadd/audio:audio_padd_sources": "scratchpadd_audio",
    "//projects/scratchpadd/audio:audio_core_sources": "scratchpadd_audio",
    "//projects/scratchpadd/audio/3ds/output:ndsp_output_sources": "scratchpadd_audio_3ds/output",
    "@miniaudio//:sources": "third_party/miniaudio",
    "@stb//:stb_easy_font.h": "third_party/stb",
    "@tinysoundfont//:sources": "third_party/tinysoundfont",
    "@spdlog//:sources": "third_party/spdlog",
}

# Cyclone's ragdoll on a Nintendo 3DS, in padds: the same app as the DS's
# (../handheld), with Cyclone in float, drawn with ScratchPadd's scene layer
# (its 3DS backend, in stereo while the 3D slider is up), and knocking as it
# hits (AudioPadd through the 3DS's DSP). Needs Docker running (see
# //tools/devkitpro); tools/fbi_install installs it on a 3DS, and `bazel run`
# opens it in Azahar.
devkitpro_3ds_app(
    name = "cyclone-3ds-ragdoll",
    srcs = ["Makefile"],
    extra_srcs = RAGDOLL_SOURCES,
)

# The same app with its sweep starting by itself (R in the app; see
# ../handheld/source/physics_sweep.h): every setting from the same shove,
# logged and ranked, unattended. Its log is cyclone-3ds-ragdoll-sweep.log.
devkitpro_3ds_app(
    name = "cyclone-3ds-ragdoll-sweep",
    srcs = ["Makefile"],
    defines = ["RAGDOLL_SWEEP_AT_START"],
    extra_srcs = RAGDOLL_SOURCES,
    icon_letters = "Sw",
)
