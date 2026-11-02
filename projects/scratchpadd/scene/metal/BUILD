load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test", "objc_library")

# The scene layer's Metal backend, for Macs: Scene::Renderer, the desktop
# extras (Desktop.hpp) and its part of the window (WindowBackend.hpp), with
# metal-cpp. Apps depend on //projects/scratchpadd/scene/desktop, which picks
# it on a Mac. See ../DESIGN.md.
cc_library(
    name = "metal",
    srcs = [
        "src/Backend.hpp",
        "src/Desktop.cpp",
        "src/MetalCpp.cpp",
        "src/Renderer.cpp",
        "src/Surface.hpp",
        "src/Window.cpp",
    ],
    hdrs = glob(["include/**/*.hpp"]),
    # Its window hooks (src/Window.cpp) are called from the window library,
    # which links after it: kept whole, so they're found
    alwayslink = True,
    strip_include_prefix = "include/",
    target_compatible_with = ["@platforms//os:macos"],
    visibility = ["//projects/scratchpadd:__subpackages__"],
    deps = [
        ":glsl_to_msl",
        ":layer",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop:window",
        "@glfw",
        "@glm",
        "@metal-cpp",
    ],
)

# The Metal layer in a GLFW window: the backend's only Objective-C
objc_library(
    name = "layer",
    srcs = ["src/Layer.mm"],
    hdrs = ["src/Layer.hpp"],
    sdk_frameworks = [
        "Cocoa",
        "Metal",
        "QuartzCore",
    ],
    target_compatible_with = ["@platforms//os:macos"],
    deps = ["@glfw"],
)

# GLSL in Metal's shading language (glslang and SPIRV-Cross), for
# ShaderPanel
cc_library(
    name = "glsl_to_msl",
    srcs = ["src/GlslToMsl.cpp"],
    hdrs = ["src/GlslToMsl.hpp"],
    strip_include_prefix = "src/",
    target_compatible_with = ["@platforms//os:macos"],
    deps = [
        "@glslang",
        "@spirv_cross",
    ],
)

# Every shader of the shader playground translated, and the result compiled
# by Metal; and a shader's uniforms found where it put them
cc_test(
    name = "GlslToMsl_test",
    size = "small",
    srcs = ["tests/GlslToMsl_test.cpp"],
    data = ["//projects/examples/shader_playground:shaders"],
    target_compatible_with = ["@platforms//os:macos"],
    deps = [
        ":glsl_to_msl",
        ":metal",
        "@gtest//:gtest_main",
    ],
)
