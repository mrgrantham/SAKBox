load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "exampleSDL2",
    srcs = ["src/exampleSDL2.cpp"],
    deps = [
        "@sdl2",
        "@fmt",
    ],
)
clang_format(name = "tabulate")