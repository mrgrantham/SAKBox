load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# Reads (and writes) WAV files with integer arithmetic only: 8- and 16-bit
# PCM and IMA-ADPCM. For the DS's AudioPadd, where float is slow (see
# include/ScratchPadd/Audio/WavReader.hpp).
cc_library(
    name = "wav_reader",
    srcs = ["src/WavReader.cpp"],
    hdrs = ["include/ScratchPadd/Audio/WavReader.hpp"],
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
)

# The same as sources, for builds outside Bazel's C++ rules (the DS apps)
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "WavReader_test",
    size = "small",
    srcs = ["tests/WavReader_test.cpp"],
    deps = [
        ":wav_reader",
        "@gtest//:gtest_main",
    ],
)
