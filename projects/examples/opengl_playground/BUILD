load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "opengl_playground",
        srcs = ["src/main.cpp"],
        deps = [
        "//projects/scratchpadd/basis:basis",
        "//projects/scratchpadd/graphics:graphics",
        "//projects/scratchpadd/messages:messages",
        "@backward",
        ],
)

clang_format(name = "opengl_playground")
