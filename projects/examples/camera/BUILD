load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "exampleCamera",
    srcs = ["src/exampleCamera.cpp"],
    deps = [
        "@sdl2",
        "@fmt",
        # "@openpnp-capture",
        "@opencv",
    ],
)
clang_format(name = "exampleCamera")