load("@rules_cc//cc:defs.bzl", "cc_library")

# miniaudio (https://miniaud.io): playback, decoding and data conversion in
# one C file. Built once here, with device output, for desktops; the 3DS
# build compiles miniaudio.c itself with MA_NO_DEVICE_IO (decoding only).
cc_library(
    name = "miniaudio",
    srcs = ["miniaudio.c"],
    hdrs = ["miniaudio.h"],
    copts = ["-w"],  # third-party code, kept as published
    include_prefix = "miniaudio",  # <miniaudio/miniaudio.h>
    # Linux loads ALSA/PulseAudio/JACK at run time, so only these libraries
    # are linked; macOS links CoreAudio at run time too.
    linkopts = select({
        "@platforms//os:linux": ["-ldl", "-lpthread", "-lm"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
)

# The sources, for builds outside Bazel's C++ rules (the 3DS)
filegroup(
    name = "sources",
    srcs = ["miniaudio.c", "miniaudio.h", "LICENSE"],
    visibility = ["//visibility:public"],
)
