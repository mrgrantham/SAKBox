load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer's OpenGL 3.3 backend, for desktops: Scene::Renderer, the
# window it draws into (Display.hpp, GLFW), and what desktops can do beyond
# the shared API (Desktop.hpp). See ../DESIGN.md.
cc_library(
    name = "gl",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = [
        "src/Backend.hpp",
        "src/Desktop.cpp",
        "src/Display.cpp",
        "src/Renderer.cpp",
    ],
    defines = ["GL_SILENCE_DEPRECATION"],
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
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/scene",
        "@glew",
        "@glfw",
        "@glm",
    ],
)

# Draws scenes into a hidden window and reads its pixels back, so it needs
# a display and OpenGL 3.3: it runs on desktops, not in the headless Linux
# check (tagged "display"; see tools/linux_check).
cc_test(
    name = "Renderer_test",
    size = "small",
    srcs = ["tests/Renderer_test.cpp"],
    tags = ["display"],
    deps = [
        ":gl",
        "@glew",
        "@gtest//:gtest_main",
    ],
)
