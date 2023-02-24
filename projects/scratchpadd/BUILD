load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_library(
    name = "scratchpadd",
    srcs = glob(["src/Graphics.cpp"]),
    hdrs = glob(["include/**/*.hpp"]),
    data = glob(["include/Graphics/GL/Shaders/*.shader"]),
    defines = [
        "SCRATCHPADD_GL",
        "GL_SILENCE_DEPRECATION",
        "PROJECT_NAME=\\\"SCRATCHPADD\\\"",
    ],
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
    deps = [
        "@boost//:lockfree",
        "@boost//:program_options",
        "@fmt",
        "@glew",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_backend_opengl3",
        "@spdlog",
    ],
)

cc_binary(
    name = "demo",
    srcs = ["src/main.cpp"],
    deps = [
        ":scratchpadd",
        "@backward-cpp",
    ],
)

clang_format(name = "flexmath")
