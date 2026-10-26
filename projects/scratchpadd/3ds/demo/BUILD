load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# ScratchPadd on a Nintendo 3DS: padds on their own threads, talking by
# messages, with AudioPadd playing through the 3DS's DSP. Needs Docker
# running (see //tools/devkitpro); `bazel run` opens it in Azahar.
devkitpro_3ds_app(
    name = "scratchpadd-3ds-demo",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
        "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
        "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
        "//projects/scratchpadd/audio:audio_padd_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio:audio_core_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio/3ds/output:ndsp_output_sources": "scratchpadd_audio_3ds/output",
        "@miniaudio//:sources": "third_party/miniaudio",
        "@tinysoundfont//:sources": "third_party/tinysoundfont",
        "@spdlog//:sources": "third_party/spdlog",
    },
)
