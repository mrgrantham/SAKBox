load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

filegroup(
    name = "shaders",
    data = glob(["shaders/**"]),
)

cc_library(
    name = "opengl_tutorial_lib",
    hdrs = glob(["include/**/*.h"]),
        srcs = ["src/Shader.cpp","src/VertexBufferObject.cpp","src/ElementBufferObject.cpp", "src/VertexArrayObject.cpp"],
        strip_include_prefix = "include/",
        deps = [
        "@glew",
        "@glfw",
        "@backward",
        "@spdlog",
        "//projects/data_dep_retriever:data_dep_retriever",
        ],
        data = [":shaders"],
)

cc_binary(
    name = "opengl_tutorial",
        srcs = ["src/main.cpp"],
        deps = [
        "@glew",
        "@glfw",
        "@backward",
        "@spdlog",
        "//projects/data_dep_retriever:data_dep_retriever",
        ":opengl_tutorial_lib",
        ],
        data = [":shaders"],
)



clang_format(name = "opengl_tutorial")