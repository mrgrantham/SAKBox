
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "fourier_net",
    # Written for an older Waffles API (GActivation.h, GLayerClassic, ...)
    # that //projects/waffles no longer has, so //... skips it until ported.
    tags = ["manual"],
    srcs = glob(["src/*.cpp","src/*.h"]),
    data = glob(["data/**"], allow_empty = True),  # .arff files read by loader.cpp (not checked in)
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "@fmt",
    ],
)

clang_format(name = "fourier_net")