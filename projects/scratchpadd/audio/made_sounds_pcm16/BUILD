load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/embed_file:embed_file.bzl", "embed_file")

# The sounds made in code (MadeSounds.hpp), rendered when the program is
# built and compiled in as 16-bit samples: for the DS, where making them in
# float takes seconds (see include/ScratchPadd/Audio/MadeSoundsPcm16.hpp)

cc_binary(
    name = "render_made_sounds",
    srcs = ["tools/RenderMadeSounds.cpp"],
    deps = ["//projects/scratchpadd/audio:audio_made_sounds"],
)

SOUNDS = ["chime", "knock", "blast", "note"]

genrule(
    name = "rendered",
    outs = [sound + ".pcm16" for sound in SOUNDS],
    # (kRate in MadeSoundsPcm16.hpp)
    cmd = "$(location :render_made_sounds) 16384 " + " ".join([
        "%s=$(location %s.pcm16)" % (sound, sound)
        for sound in SOUNDS
    ]),
    tools = [":render_made_sounds"],
)

[embed_file(
    name = sound + "_pcm16",
    src = sound + ".pcm16",
    namespace = "ScratchPadd::Audio::MadeSoundsPcm16::Bytes",
    symbol = sound,
) for sound in SOUNDS]

cc_library(
    name = "made_sounds_pcm16",
    srcs = ["src/MadeSoundsPcm16.cpp"],
    hdrs = ["include/ScratchPadd/Audio/MadeSoundsPcm16.hpp"],
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
    deps = [":%s_pcm16" % sound for sound in SOUNDS],
)

# The same as sources, the rendered sounds' generated .cpp files included,
# for builds outside Bazel's C++ rules (the DS and 3DS apps)
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]) + [":%s_pcm16_source" % sound for sound in SOUNDS],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "MadeSoundsPcm16_test",
    size = "small",
    srcs = ["tests/MadeSoundsPcm16_test.cpp"],
    deps = [
        ":made_sounds_pcm16",
        "//projects/scratchpadd/audio:audio_made_sounds",
        "@gtest//:gtest_main",
    ],
)
