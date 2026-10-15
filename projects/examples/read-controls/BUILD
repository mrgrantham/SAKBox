
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "read-controls",
    srcs = ["source/main.c"],
    # Nintendo 3DS homebrew: needs devkitPro (see Makefile), so //... skips it.
    tags = ["manual"],
    deps = [
        "@fmt",
        "@libpng",
    ],
)