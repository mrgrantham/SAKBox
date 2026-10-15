
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "hello_ml",
    # Written for an older Waffles API (GActivation.h, GLayerClassic, ...)
    # that //projects/waffles no longer has, so //... skips it until ported.
    tags = ["manual"],
    srcs = ["src/main.cpp"],
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "@fmt",
        "@backward",
    ],
)

clang_format(name = "hello_ml")