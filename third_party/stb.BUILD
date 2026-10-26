load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "stb",
    hdrs = glob(["**/*.h"]),
    visibility = ["//visibility:public"],
    include_prefix = "stb",
)
# stb_vorbis (public domain or MIT): Ogg Vorbis decoding, which
# TinySoundFont uses for compressed SoundFonts (.sf3). Compiled here; its
# declarations are included from stb_vorbis.c with STB_VORBIS_HEADER_ONLY.
cc_library(
    name = "stb_vorbis",
    srcs = ["stb_vorbis.c"],
    copts = ["-w"],  # third-party code, kept as published
    textual_hdrs = ["stb_vorbis.c"],
    include_prefix = "stb",
    visibility = ["//visibility:public"],
)
