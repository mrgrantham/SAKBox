load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")


filegroup(
    name = "shaders",
    data = glob(["include/ScratchPadd/Graphics/GL/Shaders/**"]),
)

cc_library(
    name = "graphics",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = ["src/Graphics.cpp"],
    visibility = ["//visibility:public"],
    linkopts = select({
    "@platforms//os:windows": [
        "-DEFAULTLIB:user32.lib",
        "-DEFAULTLIB:gdi32.lib",
        "-DEFAULTLIB:shell32.lib",
    ],
    "@platforms//os:macos": [
        "-framework OpenGL",
        "-framework Cocoa",
        "-framework IOKit",
        "-framework CoreFoundation",
        "-framework CoreVideo",
    ],
    "//conditions:default": [],
}),
    strip_include_prefix = "include/",

    defines = [
        "SCRATCHPADD_GL",
        "GL_SILENCE_DEPRECATION",
    ],
    deps = [
        "@glew",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_backend_opengl3",
        "//projects/scratchpadd/basis:basis",
        "//projects/scratchpadd/messages:messages",
    ],
    data = [":shaders"],
)