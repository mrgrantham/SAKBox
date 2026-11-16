load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

# ScratchPadd on a Nintendo DS: padds on their own threads (calico's), talking
# by messages, with AudioPadd playing on the DS's hardware sound channels.
# Needs Docker running (see //tools/devkitpro); `bazel run` opens it in
# melonDS.
# Two builds of one source: the demo, and its -activity build, which opens
# the bottom screen's panel on the Activity tab from the start (to see its
# pixel graphs in an emulator without pressing anything)
[devkitpro_nds_app(

    name = name,
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/debug_log:sources": "scratchpadd/debug_log",
        "//projects/scratchpadd/storage:sources": "scratchpadd/storage",
        "//projects/scratchpadd/input:sources": "scratchpadd/input",
        "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
        "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
        "//projects/scratchpadd/app_ended:sources": "scratchpadd/app_ended",
        "//projects/scratchpadd/handheld_controls:sources": "scratchpadd/handheld_controls",
        "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
        "//projects/scratchpadd/ds/platform:sources": "scratchpadd_ds/platform",
        "//projects/scratchpadd/handheld:sources": "scratchpadd/handheld",
        "//projects/scratchpadd/audio/made_sounds_pcm16:sources": "scratchpadd_audio_pcm16",
        "//projects/scratchpadd/audio:audio_padd_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio:audio_core_without_engine_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio/wav_reader:sources": "scratchpadd_audio_wav",
        "@tinysoundfont//:sources": "third_party/tinysoundfont",
        "@spdlog//:sources": "third_party/spdlog",
    },
    # Its file system (NitroFS): nitro:/sounds/*.wav and nitro:/music/tune.mid
    nitro_files = {"//projects/scratchpadd/handheld:demo_files": ""},
    defines = defines,
) for name, defines in [
    ("scratchpadd-ds-demo", []),
    ("scratchpadd-ds-demo-activity", ["SAKBOX_OPEN_ACTIVITY"]),
]]
