load("@rules_cc//cc:defs.bzl", "cc_library")

# RtMidi (https://github.com/thestk/rtmidi): MIDI input and output through
# each desktop's own MIDI system. MIT-style. On Linux it uses ALSA, so it
# needs ALSA's development package (libasound2-dev) to build.
cc_library(
    name = "rtmidi",
    srcs = ["RtMidi.cpp"],
    hdrs = ["RtMidi.h"],
    copts = ["-w"],  # third-party code, kept as published
    include_prefix = "rtmidi",  # <rtmidi/RtMidi.h>
    linkopts = select({
        "@platforms//os:macos": [
            "-framework CoreMIDI",
            "-framework CoreAudio",
            "-framework CoreFoundation",
        ],
        "@platforms//os:linux": ["-lasound", "-lpthread"],
        "@platforms//os:windows": ["winmm.lib"],
    }),
    # Which MIDI system to use (only RtMidi.cpp reads these). On ALSA, no
    # timestamps from a sequencer queue: opening a port starts that queue
    # asynchronously, and a note sent before it's running is lost (about 1
    # in 100 MidiInputPadd_test runs). MidiInputPadd doesn't use them.
    local_defines = select({
        "@platforms//os:macos": ["__MACOSX_CORE__"],
        "@platforms//os:linux": ["__LINUX_ALSA__", "AVOID_TIMESTAMPING"],
        "@platforms//os:windows": ["__WINDOWS_MM__"],
    }),
    visibility = ["//visibility:public"],
)
