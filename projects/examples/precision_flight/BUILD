load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "precision_flight",
    srcs = ["src/main.cpp"],
    deps = [
        "//projects/flexmath",
        "@glew",
        "@glfw",
        "@glm",
        "@imgui",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_backend_opengl3",
    ],
)

clang_format(name = "precision_flight")
