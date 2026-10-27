load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

# Cyclone's ragdoll on a Nintendo DS, in padds: the physics on its own
# thread, timed, drawn by the DS's 3D hardware, and knocking as it hits
# (AudioPadd on the DS's hardware sound channels). Needs Docker running (see
# //tools/devkitpro); `bazel run` opens it in melonDS.
devkitpro_nds_app(
    name = "cyclone-ds-ragdoll",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/cyclone:sources": "cyclone",
        "//projects/flexmath:fixed_point_sources": "flexmath",
        "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
        "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
        "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
        "//projects/scratchpadd/ds/platform:sources": "scratchpadd_ds/platform",
        "//projects/scratchpadd/audio/made_sounds_pcm16:sources": "scratchpadd_audio_pcm16",
        "//projects/scratchpadd/audio:audio_padd_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio:audio_core_without_engine_sources": "scratchpadd_audio",
        "//projects/scratchpadd/audio/wav_reader:sources": "scratchpadd_audio_wav",
        "@tinysoundfont//:sources": "third_party/tinysoundfont",
        "@spdlog//:sources": "third_party/spdlog",
    },
)
