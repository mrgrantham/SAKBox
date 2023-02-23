load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "scratchpadd",
    srcs = glob(["src/Graphics.cpp"]),
    hdrs = glob(["include/**/*.hpp",]),
    data = glob(["include/Graphics/GL/Shaders/*.shader"]),
    strip_include_prefix = "include/",
        deps = [
        "@spdlog",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_backend_opengl3",
        "@boost//:program_options",
        "@boost//:lockfree",
        "@fmt",
        "@glew",
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
    defines = ["SCRATCHPADD_GL","GL_SILENCE_DEPRECATION","PROJECT_NAME=\\\"SCRATCHPADD\\\""],
)

cc_binary(
    name = "demo",
    srcs = ["src/main.cpp"],
    deps = [
        ":scratchpadd",
        # "@backward-cpp",
    ],
)