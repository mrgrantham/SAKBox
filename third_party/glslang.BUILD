load("@rules_cc//cc:defs.bzl", "cc_library")

# glslang, Khronos' GLSL compiler, as a library that compiles GLSL to SPIR-V
# (for the Metal backend's shader translation): without HLSL and without
# SPIRV-Tools' optimiser, as its CMake build makes it with ENABLE_HLSL and
# ENABLE_OPT off. Its sources, as its BUILD.gn lists them.

# The version header its CMake and GN builds generate from build_info.h.tmpl
# (16.6.0, matching MODULE.bazel)
genrule(
    name = "build_info",
    srcs = ["build_info.h.tmpl"],
    outs = ["glslang/build_info.h"],
    cmd = "sed -e 's/@major@/16/' -e 's/@minor@/6/' -e 's/@patch@/0/' -e 's/@flavor@//' $< > $@",
)

cc_library(
    name = "glslang",
    srcs = glob(
        [
            "SPIRV/*.cpp",
            "glslang/GenericCodeGen/*.cpp",
            "glslang/MachineIndependent/**/*.cpp",
            "glslang/MachineIndependent/**/*.h",
            "glslang/ResourceLimits/ResourceLimits.cpp",
        ],
        exclude = ["SPIRV/SpvTools.cpp"],
    ) + select({
        "@platforms//os:windows": ["glslang/OSDependent/Windows/ossource.cpp"],
        "//conditions:default": ["glslang/OSDependent/Unix/ossource.cpp"],
    }),
    hdrs = glob([
        "SPIRV/*.h",
        "SPIRV/spirv.hpp11",
        "glslang/Include/*.h",
        "glslang/HLSL/*.h",
        "glslang/OSDependent/osinclude.h",
        "glslang/Public/*.h",
        "glslang/MachineIndependent/*.h",
    ]) + [":build_info"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": [
            "-w",  # its own code: its warnings aren't ours to fix
            # As its own builds compile it: it does arithmetic between
            # different enum types, which C++26 (the repo's) no longer allows
            "-std=c++17",
        ],
    }),
    defines = ["ENABLE_SPIRV=1"],
    includes = ["."],
    local_defines = select({
        "@platforms//os:windows": ["GLSLANG_OSINCLUDE_WIN32"],
        "//conditions:default": ["GLSLANG_OSINCLUDE_UNIX"],
    }),
    linkopts = select({
        "@platforms//os:linux": ["-lpthread"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
)
