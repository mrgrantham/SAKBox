load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# The ScratchPadd audio core on a Nintendo 3DS: runs the core's checks
# (mixing, volumes, pan, pitch, loops, resampling, music streamed from the
# SD card, both instruments, MIDI timing) on the 3DS's own CPU, mixing
# offline, and times a full mix. The results show on the top screen. Needs
# Docker running (see //tools/devkitpro); `bazel run` opens it in Azahar.
devkitpro_3ds_app(
    name = "scratchpadd-audio-selftest",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/audio:audio_core_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio:midi_test_data": "scratchpadd_audio",
        "@miniaudio//:sources": "third_party/miniaudio",
        "@tinysoundfont//:sources": "third_party/tinysoundfont",
    },
)
