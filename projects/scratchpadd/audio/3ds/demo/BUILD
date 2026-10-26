load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# ScratchPadd audio playing on a Nintendo 3DS, through its DSP: a tune on
# the built-in instrument and a chime, by themselves, with the output's
# figures (buffers, underruns, mix time, voices) on the top screen. Needs
# Docker running (see //tools/devkitpro); `bazel run` opens it in Azahar.
devkitpro_3ds_app(
    name = "scratchpadd-audio-3ds-demo",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/audio:audio_core_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio/3ds/output:ndsp_output_sources": "scratchpadd_audio_3ds/output",
        "@miniaudio//:sources": "third_party/miniaudio",
        "@tinysoundfont//:sources": "third_party/tinysoundfont",
    },
)
