load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

# What the ScratchPadd demos for the 3DS and the DS share (HandheldDemo.hpp),
# as sources for their devkitPro Makefiles
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//projects/scratchpadd:__subpackages__"],
)

# The files the DS demo carries in its file system (NitroFS), written when
# it's built: sounds/chime.wav (16-bit), sounds/knock.wav (8-bit),
# sounds/blast.wav (IMA-ADPCM) and music/tune.mid
cc_binary(
    name = "write_demo_files",
    srcs = ["tools/WriteDemoFiles.cpp"],
    deps = [
        "//projects/scratchpadd/audio:audio_made_sounds",
        "//projects/scratchpadd/audio/wav_reader",
    ],
)

genrule(
    name = "demo_files",
    outs = [
        "music/tune.mid",
        "sounds/blast.wav",
        "sounds/chime.wav",
        "sounds/knock.wav",
    ],
    cmd = "$(location :write_demo_files) $(RULEDIR)",
    tools = [":write_demo_files"],
    visibility = ["//projects/scratchpadd:__subpackages__"],
)

cc_test(
    name = "DemoFiles_test",
    size = "small",
    srcs = ["tests/DemoFiles_test.cpp"],
    data = [":demo_files"],
    deps = [
        "//projects/scratchpadd/audio:audio_core",
        "//projects/scratchpadd/audio/wav_reader",
        "@gtest//:gtest_main",
    ],
)
