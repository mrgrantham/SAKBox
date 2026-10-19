load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

filegroup(
    name = "shaders",
    data = glob(["shaders/**"],allow_empty = False),
)

cc_binary(
    name = "shader_playground",
        srcs = ["src/main.cpp"],
        deps = [
        "//projects/scratchpadd/basis:basis",
        "//projects/scratchpadd/graphics:graphics",
        "//projects/scratchpadd/messages:messages",
        "@backward",
        ],
        data = [":shaders"],
)
