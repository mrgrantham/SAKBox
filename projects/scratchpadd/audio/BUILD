load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# How a voice plays (volume, pan, pitch, loop) and the mix groups: shared by
# the engine and the messages that drive it
cc_library(
    name = "audio_voice",
    hdrs = ["core/include/ScratchPadd/Audio/Voice.hpp"],
    strip_include_prefix = "core/include/",
    visibility = ["//visibility:public"],
)

# The audio core: sounds, voices, the mixer and music streaming, talking to
# an Output (see README). Plain C++ with no other ScratchPadd dependency, so
# the same code builds for the 3DS. Decodes with miniaudio.
cc_library(
    name = "audio_core",
    srcs = glob(["core/src/*.cpp"]),
    hdrs = glob(
        ["core/include/**/*.hpp"],
        exclude = ["core/include/ScratchPadd/Audio/Voice.hpp"],
    ),
    strip_include_prefix = "core/include/",
    visibility = ["//visibility:public"],
    deps = [
        ":audio_voice",
        "@miniaudio",
    ],
)

# The core's sources, for builds outside Bazel's C++ rules (the 3DS)
filegroup(
    name = "audio_core_sources",
    srcs = glob(["core/**"]),
    visibility = ["//visibility:public"],
)

# Playback through the system's sound device (desktops), with miniaudio
cc_library(
    name = "audio_device",
    srcs = ["device/src/DeviceOutput.cpp"],
    hdrs = ["device/include/ScratchPadd/Audio/DeviceOutput.hpp"],
    strip_include_prefix = "device/include/",
    visibility = ["//visibility:public"],
    deps = [
        ":audio_core",
        "@miniaudio",
    ],
)

# The messages AudioPadd handles: all a padd that plays sounds needs
cc_library(
    name = "audio_messages",
    hdrs = ["messages/include/ScratchPadd/Audio/Messages.hpp"],
    strip_include_prefix = "messages/include/",
    visibility = ["//visibility:public"],
    deps = [":audio_voice"],
)

# AudioPadd: the engine as a padd, played with messages
cc_library(
    name = "audio",
    hdrs = glob(["include/**/*.hpp"]),
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":audio_core",
        ":audio_device",
        ":audio_messages",
        "//projects/scratchpadd/basis",
    ],
)

cc_test(
    name = "AudioCore_test",
    size = "small",
    srcs = ["tests/AudioCore_test.cpp"],
    deps = [
        ":audio_core",
        "@gtest//:gtest_main",
        "@miniaudio",
    ],
)

cc_test(
    name = "AudioPadd_test",
    size = "small",
    srcs = ["tests/AudioPadd_test.cpp"],
    deps = [
        ":audio",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
        "@miniaudio",
    ],
)
