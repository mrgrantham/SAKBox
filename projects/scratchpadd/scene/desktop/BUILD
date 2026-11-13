load("@bazel_skylib//lib:selects.bzl", "selects")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer on desktops. Apps depend on :desktop, which is the desktop
# backend the build uses; it brings the window (Display.hpp) and the desktop
# extras (Desktop.hpp). See ../DESIGN.md.
cc_library(
    name = "desktop",
    visibility = ["//visibility:public"],
    deps = select({
        ":metal": ["//projects/scratchpadd/scene/metal"],
        ":vulkan": ["//projects/scratchpadd/scene/vulkan"],
        "//conditions:default": ["//projects/scratchpadd/scene/gl"],
    }),
)

# Which desktop backend: auto (Metal on a Mac, OpenGL elsewhere), opengl,
# metal (Macs only) or vulkan (anywhere: MoltenVK on a Mac), for the whole
# build:
#   bazel run //projects/examples/blender_model --//projects/scratchpadd/scene/desktop:backend=opengl
string_flag(
    name = "backend",
    build_setting_default = "auto",
    values = [
        "auto",
        "metal",
        "opengl",
        "vulkan",
    ],
)

config_setting(
    name = "backend_auto",
    flag_values = {":backend": "auto"},
)

config_setting(
    name = "backend_metal",
    flag_values = {":backend": "metal"},
)

# Whether this build's desktop backend is Vulkan, for what differs with it
# (debug_ui's ImGui)
config_setting(
    name = "vulkan",
    flag_values = {":backend": "vulkan"},
    visibility = ["//projects/scratchpadd:__subpackages__"],
)

selects.config_setting_group(
    name = "auto_on_a_mac",
    match_all = [
        ":backend_auto",
        "@platforms//os:macos",
    ],
)

# Whether this build's desktop backend is Metal, for what differs with it
# (debug_ui's ImGui)
selects.config_setting_group(
    name = "metal",
    match_any = [
        ":backend_metal",
        ":auto_on_a_mac",
    ],
    visibility = ["//projects/scratchpadd:__subpackages__"],
)

# What every desktop backend shares, for the backends: the window (GLFW),
# its keyboard and mouse, and the backend hooks it calls
# (WindowBackend.hpp); the extras' API (Desktop.hpp), which each backend
# defines; and the overlay's text (//projects/scratchpadd/scene/overlay_text,
# shared with the 3DS)
cc_library(
    name = "window",
    srcs = ["src/Display.cpp"],
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
        "//projects/scratchpadd/input",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/overlay_text",
        "@glfw",
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
        "//projects/expected_capabilities",
        "//projects/scratchpadd/scene/" + package,
        "@gtest//:gtest_main",
    ],
) for backend, package, compatible in [
    ("opengl", "gl", []),
    ("metal", "metal", ["@platforms//os:macos"]),
    ("vulkan", "vulkan", []),
]]
