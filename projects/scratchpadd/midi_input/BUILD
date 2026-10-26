load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The MidiInput message: all a padd needs to hear MIDI from keyboards
cc_library(
    name = "midi_input_messages",
    hdrs = ["include/ScratchPadd/MidiInput/MidiInput.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/audio:audio_midi_event"],
)

# MidiInputPadd: MIDI from keyboards and controllers, with RtMidi (desktops)
cc_library(
    name = "midi_input",
    hdrs = ["include/MidiInputPadd.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":midi_input_messages",
        "//projects/scratchpadd/audio:audio_messages",
        "//projects/scratchpadd/basis",
        "@rtmidi",
    ],
)

cc_test(
    name = "MidiInputPadd_test",
    size = "small",
    srcs = ["tests/MidiInputPadd_test.cpp"],
    data = ["tests/tsan_suppressions.txt"],
    # Used when the test is built with ThreadSanitizer (see the file)
    env = {"TSAN_OPTIONS": "suppressions=projects/scratchpadd/midi_input/tests/tsan_suppressions.txt"},
    deps = [
        ":midi_input",
        "//projects/scratchpadd/audio",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
        "@rtmidi",
    ],
)
