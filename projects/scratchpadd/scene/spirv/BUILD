load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# GLSL compiled to SPIR-V (glslang), and an OpenGL shader's uniforms found
# in it (SPIRV-Cross): for the desktop backends that take SPIR-V, Vulkan and
# (through SPIRV-Cross) Metal. See ../DESIGN.md.
cc_library(
    name = "glsl_to_spirv",
    srcs = ["src/GlslToSpirv.cpp"],
    hdrs = ["include/ScratchPadd/Scene/Spirv/GlslToSpirv.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//projects/scratchpadd/scene:__subpackages__"],
    deps = [
        "@glslang",
        "@spirv_cross",
    ],
)

# Every shader of the shader playground compiles; a shader's loose uniforms
# are found; a mistake is reported
cc_test(
    name = "GlslToSpirv_test",
    size = "small",
    srcs = ["tests/GlslToSpirv_test.cpp"],
    data = ["//projects/examples/shader_playground:shaders"],
    deps = [
        ":glsl_to_spirv",
        "@gtest//:gtest_main",
    ],
)
