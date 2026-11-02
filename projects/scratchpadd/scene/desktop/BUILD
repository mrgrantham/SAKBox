load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer on desktops. Apps depend on :desktop, which is the desktop
# backend the build uses (OpenGL); it brings the window (Display.hpp) and
# the desktop extras (Desktop.hpp). See ../DESIGN.md.
cc_library(
    name = "desktop",
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/scene/gl"],
)

# What every desktop backend shares, for the backends: the window (GLFW),
# its keyboard and mouse, and the backend hooks it calls
# (WindowBackend.hpp); the extras' API (Desktop.hpp), which each backend
# defines; and the overlay's text (stb_easy_font)
cc_library(
    name = "window",
    srcs = [
        "src/Display.cpp",
        "src/OverlayText.cpp",
    ],
    hdrs = glob(["include/**/*.hpp"]),
    linkopts = select({
        "@platforms//os:windows": [
            "-DEFAULTLIB:user32.lib",
            "-DEFAULTLIB:gdi32.lib",
            "-DEFAULTLIB:shell32.lib",
        ],
        "@platforms//os:macos": [
            "-framework Cocoa",
            "-framework IOKit",
            "-framework CoreFoundation",
            "-framework CoreVideo",
        ],
        "//conditions:default": [],
    }),
    strip_include_prefix = "include/",
    visibility = ["//projects/scratchpadd/scene:__subpackages__"],
    deps = [
        "//projects/scratchpadd/scene",
        "@glfw",
        "@stb",
    ],
)

# The overlay's text (stb_easy_font) on its own: no window needed
cc_test(
    name = "OverlayText_test",
    size = "small",
    srcs = ["tests/OverlayText_test.cpp"],
    deps = [
        ":window",
        "@gtest//:gtest_main",
    ],
)

# Each backend drawing scenes into a hidden window, its pixels read back, so
# it needs a display: it runs on desktops, not in the headless Linux check
# (tagged "display"; see tools/linux_check)
[cc_test(
    name = "Renderer_" + backend + "_test",
    size = "small",
    srcs = ["tests/Renderer_test.cpp"],
    tags = ["display"],
    target_compatible_with = compatible,
    deps = [
        "//projects/scratchpadd/scene/" + package,
        "@gtest//:gtest_main",
    ],
) for backend, package, compatible in [
    ("opengl", "gl", []),
]]
