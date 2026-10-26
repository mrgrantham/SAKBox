load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# How a voice plays (volume, pan, pitch, loop) and the mix groups: shared by
# the engine and the messages that drive it
cc_library(
    name = "audio_voice",
    hdrs = ["core/include/ScratchPadd/Audio/Voice.hpp"],
    strip_include_prefix = "core/include/",
    visibility = ["//visibility:public"],
)

# One MIDI message (a note, a controller, ...): shared by the engine and the
# messages that play MIDI
cc_library(
    name = "audio_midi_event",
    hdrs = ["core/include/ScratchPadd/Audio/MidiEvent.hpp"],
    strip_include_prefix = "core/include/",
    visibility = ["//visibility:public"],
)

# Sounds made in code (a chime, a knock, a blast), for programs to play
# without sound files. No dependencies.
cc_library(
    name = "audio_made_sounds",
    srcs = ["core/src/MadeSounds.cpp"],
    hdrs = ["core/include/ScratchPadd/Audio/MadeSounds.hpp"],
    strip_include_prefix = "core/include/",
    visibility = ["//visibility:public"],
)

# The audio core: sounds, voices, the mixer and music streaming, talking to
# an Output (see README). Plain C++ with no other ScratchPadd dependency, so
# the same code builds for the 3DS. Decodes with miniaudio.
cc_library(
    name = "audio_core",
    srcs = glob(
        ["core/src/*.cpp"],
        exclude = ["core/src/MadeSounds.cpp"],
    ),
    hdrs = glob(
        ["core/include/**/*.hpp"],
        exclude = [
            "core/include/ScratchPadd/Audio/MadeSounds.hpp",
            "core/include/ScratchPadd/Audio/MidiEvent.hpp",
            "core/include/ScratchPadd/Audio/Voice.hpp",
        ],
    ),
    # Compressed SoundFonts (.sf3), with stb_vorbis (the 3DS build leaves it out)
    local_defines = ["SCRATCHPADD_AUDIO_SF3"],
    strip_include_prefix = "core/include/",
    visibility = ["//visibility:public"],
    deps = [
        ":audio_midi_event",
        ":audio_voice",
        "@miniaudio",
        "@stb//:stb_vorbis",
        "@tinysoundfont",
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
    deps = [
        ":audio_midi_event",
        ":audio_voice",
    ],
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

# Helpers the core's tests share
cc_library(
    name = "audio_test_helpers",
    testonly = True,
    hdrs = [
        "tests/CountAllocations.hpp",
        "tests/MidiTestData.hpp",
        "tests/OfflineEngine.hpp",
    ],
    strip_include_prefix = "tests/",
    deps = [
        ":audio_core",
        "@gtest",
    ],
)

# MidiTestData.hpp, for the 3DS self-test
filegroup(
    name = "midi_test_data",
    srcs = ["tests/MidiTestData.hpp"],
    visibility = ["//projects/scratchpadd/audio/3ds:__subpackages__"],
)

cc_test(
    name = "AudioCore_test",
    size = "small",
    srcs = ["tests/AudioCore_test.cpp"],
    deps = [
        ":audio_core",
        ":audio_test_helpers",
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
        ":audio_test_helpers",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
        "@miniaudio",
    ],
)

cc_test(
    name = "Midi_test",
    size = "small",
    srcs = ["tests/Midi_test.cpp"],
    deps = [
        ":audio_core",
        ":audio_test_helpers",
        "@gtest//:gtest_main",
    ],
)

# Renders the built-in instrument (and FluidR3, to compare) to WAV files in
# .audio-samples/, to listen to: bazel run :render_instrument_samples
cc_binary(
    name = "render_instrument_samples",
    srcs = ["tools/RenderInstrumentSamples.cpp"],
    data = ["@fluidr3_mono_soundfont//file"],
    deps = [
        ":audio_core",
        "//projects/data_dep_retriever",
        "@miniaudio",
    ],
)

cc_test(
    name = "MadeSounds_test",
    size = "small",
    srcs = ["tests/MadeSounds_test.cpp"],
    deps = [
        ":audio_made_sounds",
        "@gtest//:gtest_main",
    ],
)

# AudioPadd (both kinds: see include/AudioPadd.hpp) and the messages it
# handles, as sources, for builds outside Bazel's C++ rules (the 3DS and DS
# apps; see //tools/devkitpro). With :audio_core_sources on the 3DS, or
# :audio_core_without_engine_sources on the DS.
filegroup(
    name = "audio_padd_sources",
    srcs = glob(["include/**", "messages/include/**"]),
    visibility = ["//visibility:public"],
)

# The parts of the core the DS's AudioPadd needs, without the engine (which
# it doesn't use): the messages' Voice.hpp and MidiEvent.hpp, the sounds
# made in code, and the MIDI file reader (with TinySoundFont's tml.h)
filegroup(
    name = "audio_core_without_engine_sources",
    srcs = [
        "core/include/ScratchPadd/Audio/MadeSounds.hpp",
        "core/include/ScratchPadd/Audio/MidiEvent.hpp",
        "core/include/ScratchPadd/Audio/MidiSequence.hpp",
        "core/include/ScratchPadd/Audio/Voice.hpp",
        "core/src/MadeSounds.cpp",
        "core/src/MidiSequence.cpp",
    ],
    visibility = ["//visibility:public"],
)
