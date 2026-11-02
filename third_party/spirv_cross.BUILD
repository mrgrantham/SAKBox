load("@rules_cc//cc:defs.bzl", "cc_library")

# SPIRV-Cross, which turns SPIR-V into other shading languages: here only
# Metal's (and the GLSL backend it's built on), for the Metal backend's
# shader translation
cc_library(
    name = "spirv_cross",
    srcs = [
        "spirv_cfg.cpp",
        "spirv_cross.cpp",
        "spirv_cross_parsed_ir.cpp",
        "spirv_cross_util.cpp",
        "spirv_glsl.cpp",
        "spirv_msl.cpp",
        "spirv_parser.cpp",
    ],
    hdrs = [
        "GLSL.std.450.h",
        "NonSemanticShaderDebugInfo100.h",
        "spirv.h",
        "spirv.hpp",
        "spirv_cfg.hpp",
        "spirv_common.hpp",
        "spirv_cross.hpp",
        "spirv_cross_containers.hpp",
        "spirv_cross_error_handling.hpp",
        "spirv_cross_parsed_ir.hpp",
        "spirv_cross_util.hpp",
        "spirv_glsl.hpp",
        "spirv_msl.hpp",
        "spirv_parser.hpp",
    ],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-w"],  # its own code
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
)
