
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "examplePng",
    srcs = glob(["src/pngtest.cpp"]),
    includes = glob(["include/*.h"]),
    data = glob(["data/**"]),
    deps = [
        "@fmt",
        "@libpng",
    ],
)

clang_format(name = "examplePng")