load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

filegroup(
    name = "shaders",
    data = glob(["resources/shaders/**"]),
)

filegroup(
    name = "textures",
    data = glob(["resources/textures/**"]),
)

cc_library(
    name = "opengl_tutorial_lib",
    hdrs = glob(["include/**/*.h"]),
        srcs = ["src/ShaderProgram.cpp","src/VertexBufferObject.cpp","src/ElementBufferObject.cpp", "src/VertexArrayObject.cpp", "src/Texture.cpp"],
        strip_include_prefix = "include/",
        deps = [
        "@glew",
        "@glfw",
        "@backward",
        "@spdlog",
        "//projects/data_dep_retriever:data_dep_retriever",
        "@stb"
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
        data = [":shaders",":textures"],
)



clang_format(name = "opengl_tutorial")