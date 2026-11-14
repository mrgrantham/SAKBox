load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

filegroup(
    name = "shaders",
    data = glob(["shaders/**"],allow_empty = False),
    # The scene layer's shader tests compile them all
    visibility = [
        "//projects/scratchpadd/scene/metal:__pkg__",
        "//projects/scratchpadd/scene/spirv:__pkg__",
        "//projects/scratchpadd/scene/vulkan:__pkg__",
    ],
)

cc_binary(
    name = "shader_playground",
        srcs = ["src/main.cpp"],
        deps = [
        "//projects/scratchpadd/debug_log",
        "//projects/scratchpadd/storage:storage_padd",
        "//projects/scratchpadd/basis:basis",
        "//projects/data_dep_retriever",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop",
        "//projects/scratchpadd/messages:messages",
        "@backward",
        ],
        data = [":shaders"],
)
