load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The scene layer's Vulkan backend: Scene::Renderer, the desktop extras
# (Desktop.hpp) and its part of the window (WindowBackend.hpp). Chosen with
# --//projects/scratchpadd/scene/desktop:backend=vulkan; apps depend on
# //projects/scratchpadd/scene/desktop, which picks it then. Vulkan's
# functions are loaded when the program runs (volk): the system's Vulkan
# loader on Linux and Windows, MoltenVK (Vulkan on Metal, from the
# runfiles) on Macs. See ../DESIGN.md.
cc_library(
    name = "vulkan",
    srcs = [
        "src/Backend.hpp",
        "src/Desktop.cpp",
        "src/Device.cpp",
        "src/Device.hpp",
        "src/Renderer.cpp",
        "src/Surface.hpp",
        "src/Window.cpp",
    ],
    hdrs = glob(["include/**/*.hpp"]),
    data = select({
        "@platforms//os:macos": ["@moltenvk"],
        "//conditions:default": [],
    }),
    # Its window hooks (src/Window.cpp) are called from the window library,
    # which links after it: kept whole, so they're found
    alwayslink = True,
    strip_include_prefix = "include/",
    visibility = ["//projects/scratchpadd:__subpackages__"],
    deps = [
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop:window",
        "//projects/scratchpadd/scene/spirv:glsl_to_spirv",
        "@glfw",
        "@glm",
        "@volk",
    ] + select({
        "@platforms//os:macos": ["@rules_cc//cc/runfiles"],
        "//conditions:default": [],
    }),
)

# ShaderPanels on Vulkan: every shader of the shader playground made into a
# pipeline and drawn. Needs no window; skipped where there's no Vulkan
# (see projects/expected_capabilities).
cc_test(
    name = "ShaderPanel_test",
    size = "small",
    srcs = ["tests/ShaderPanel_test.cpp"],
    data = ["//projects/examples/shader_playground:shaders"],
    deps = [
        ":vulkan",
        "//projects/expected_capabilities",
        "@gtest//:gtest_main",
    ],
)
