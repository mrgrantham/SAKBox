load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# The ScratchPadd audio core on a Nintendo 3DS: runs the core's checks
# (mixing, volumes, pan, pitch, loops, resampling, music streamed from the
# SD card) on the 3DS's own CPU, mixing offline, and times a full mix. The
# results show on the top screen. Needs Docker running (see
# //tools/devkitpro).
devkitpro_3ds_app(
    name = "scratchpadd-audio-selftest",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/scratchpadd/audio:audio_core_sources": "scratchpadd_audio",
        "@miniaudio//:sources": "third_party/miniaudio",
    },
)
