load("@rules_cc//cc:defs.bzl", "cc_library")

# The scene layer's OpenGL 3.3 backend, for desktops: Scene::Renderer, the
# desktop extras (Desktop.hpp) and its part of the window (WindowBackend.hpp).
# Apps depend on //projects/scratchpadd/scene/desktop, which picks a
# backend. See ../DESIGN.md.
cc_library(
    name = "gl",
    srcs = [
        "src/Backend.hpp",
        "src/Desktop.cpp",
        "src/Renderer.cpp",
        "src/Window.cpp",
    ],
    # Its window hooks (src/Window.cpp) are called from the window library,
    # which links after it: kept whole, so GNU ld finds them
    alwayslink = True,
    defines = ["GL_SILENCE_DEPRECATION"],
    linkopts = select({
        "@platforms//os:macos": ["-framework OpenGL"],
        "//conditions:default": [],
    }),
    visibility = ["//projects/scratchpadd:__subpackages__"],
    deps = [
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop:window",
        "@glew",
        "@glfw",
        "@glm",
    ],
)
