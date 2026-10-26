load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

# ScratchPadd on a Nintendo DS: padds on their own threads (calico's), talking
# by messages, with AudioPadd playing on the DS's hardware sound channels.
# Needs Docker running (see //tools/devkitpro); `bazel run` opens it in
# melonDS.
devkitpro_nds_app(
    name = "scratchpadd-ds-demo",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
        "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
        "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
        "//projects/scratchpadd/ds/platform:sources": "scratchpadd_ds/platform",
        "//projects/scratchpadd/handheld:sources": "scratchpadd/handheld",
        "//projects/scratchpadd/audio/made_sounds_pcm16:sources": "scratchpadd_audio_pcm16",
        "//projects/scratchpadd/audio:audio_padd_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio:audio_core_without_engine_sources": "scratchpadd_audio",
        "@spdlog//:sources": "third_party/spdlog",
    },
)
