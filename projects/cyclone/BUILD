load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load(":real.bzl", "real_binary", "real_test")

# Cyclone physics engine by Ian Millington (MIT, see LICENSE). Originally from
# https://github.com/idmillington/cyclone-physics (master, Jan 2024); maintained
# in SAKBox since upstream stopped.

package(default_visibility = ["//visibility:public"])

# The type of cyclone::real (precision.h) in every target the build makes:
#   bazel run //projects/cyclone:ragdoll --//projects/cyclone:real=flex_point16
# double (the default); float; flex_real (flexmath's FlexReal, a double that
# snaps to a precision that can change as it runs); flex_point or
# flex_point16 (flexmath's Q32.32 and Q16.16 fixed point). real_binary and
# real_test (real.bzl) build one target with a given type.
REAL_TYPES = ["double", "float", "flex_real", "flex_point", "flex_point16"]

string_flag(
    name = "real",
    build_setting_default = "double",
    values = REAL_TYPES,
)

[config_setting(
    name = "real_" + real,
    flag_values = {":real": real},
) for real in REAL_TYPES]

# The define precision.h picks real by, and propagates to every dependent
REAL_DEFINES = select({
    ":real_float": ["CYCLONE_SINGLE_PRECISION"],
    ":real_flex_real": ["CYCLONE_FLEX_REAL"],
    ":real_flex_point": ["CYCLONE_FLEX_POINT"],
    ":real_flex_point16": ["CYCLONE_FLEX_POINT16"],
    "//conditions:default": [],
})

# flexmath's types; double's maths functions are flexmath's correctly
# rounded ones
REAL_DEPS = select({
    ":real_flex_real": ["//projects/flexmath"],
    ":real_flex_point": ["//projects/flexmath"],
    ":real_flex_point16": ["//projects/flexmath"],
    "//conditions:default": [],
}) + ["//projects/flexmath:correctly_rounded"]

# The engine, with the real type the build picks (:real)
cc_library(
    name = "cyclone",
    srcs = ["src/body.cpp","src/collide_coarse.cpp","src/collide_fine.cpp","src/contacts.cpp","src/core.cpp","src/fgen.cpp","src/joints.cpp","src/particle.cpp","src/pcontacts.cpp","src/pfgen.cpp","src/plinks.cpp","src/pworld.cpp","src/random.cpp","src/world.cpp"],
    hdrs = ["include/cyclone/body.h","include/cyclone/cyclone.h","include/cyclone/collide_coarse.h","include/cyclone/collide_fine.h","include/cyclone/contacts.h","include/cyclone/core.h","include/cyclone/fgen.h","include/cyclone/joints.h","include/cyclone/particle.h","include/cyclone/precision.h","include/cyclone/pcontacts.h","include/cyclone/pfgen.h","include/cyclone/plinks.h","include/cyclone/pworld.h","include/cyclone/random.h","include/cyclone/world.h"],
    copts = ["-O2", "-fPIC"],
    defines = REAL_DEFINES,
    deps = REAL_DEPS,
    strip_include_prefix = "include",
)

cc_library(
    name = "timing",
    srcs = ["src/demos/timing.cpp"],
    hdrs = ["src/demos/timing.h"],
    copts = ["-O2", "-fPIC"],
)

# The rigid-body demos' physics clock, shared with fixed_steps_test
cc_library(
    name = "fixed_steps",
    hdrs = ["src/demos/fixed_steps.h"],
    strip_include_prefix = "src/demos",
)

# Which hits to knock for (no audio): for the demos' framework, and for code
# outside the demos, such as the Cyclone padds example
cc_library(
    name = "impact_knocks",
    srcs = ["src/demos/impact_knocks.cpp"],
    hdrs = ["src/demos/impact_knocks.h"],
    strip_include_prefix = "src/demos",
    deps = [":cyclone"],
)

# The demos' framework, with the real type the build picks (:real)
cc_library(
    name = "app",
    srcs = [
        "src/demos/app.cpp",
        "src/demos/impact_sounds.cpp",
        "src/demos/shapes.cpp",
        "src/demos/window.cpp",
    ],
    hdrs = [
        "src/demos/app.h",
        "src/demos/impact_sounds.h",
        "src/demos/ogl_headers.h",
        "src/demos/shapes.h",
        "src/demos/window.h",
    ],
    defines = ["GL_SILENCE_DEPRECATION"],
    # The demos open a GLFW window and draw with legacy OpenGL and GLU (GLFW
    # links OpenGL on macOS); the engine itself has no platform dependency.
    linkopts = select({
        "@platforms//os:linux": ["-lGL", "-lGLU"],
        "@platforms//os:windows": ["opengl32.lib", "glu32.lib"],
        "//conditions:default": [],
    }),
    strip_include_prefix = "src",
    deps = [
        ":cyclone",
        ":fixed_steps",
        ":impact_knocks",
        ":timing",
        # SAKBox: sounds for the rigid-body demos (impact_sounds.h)
        "//projects/scratchpadd/audio:audio_core",
        "//projects/scratchpadd/audio:audio_device",
        "//projects/scratchpadd/audio:audio_made_sounds",
        "@glfw",
        "@stb",
    ],
)

cc_binary(
    name = "ballistic",
    srcs = ["src/demos/ballistic/ballistic.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "bigballistic",
    srcs = ["src/demos/bigballistic/bigballistic.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "blob",
    srcs = ["src/demos/blob/blob.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "bridge",
    srcs = ["src/demos/bridge/bridge.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "explosion",
    srcs = ["src/demos/explosion/explosion.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "stack",
    srcs = ["src/demos/stack/stack.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "fireworks",
    srcs = ["src/demos/fireworks/fireworks.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "flightsim",
    srcs = ["src/demos/flightsim/flightsim.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "fracture",
    srcs = ["src/demos/fracture/fracture.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

cc_binary(
    name = "platform",
    srcs = ["src/demos/platform/platform.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)

# The ragdoll's physics, shared by the demo and ragdoll_test
cc_library(
    name = "ragdoll_physics",
    hdrs = ["src/demos/ragdoll/ragdoll_physics.h"],
    strip_include_prefix = "src/demos/ragdoll",
    deps = [":cyclone"],
)

cc_binary(
    name = "ragdoll",
    srcs = ["src/demos/ragdoll/ragdoll.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
        ":ragdoll_physics",
    ],
)

cc_binary(
    name = "sailboat",
    srcs = ["src/demos/sailboat/sailboat.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)
# The flight sim on FlexReal, whatever :real is set to
real_binary(
    name = "flightsim_flex",
    real = "flex_real",
    target = ":flightsim",
)

cc_test(
    name = "flex_test_on_flex_real",
    size = "small",
    srcs = ["tests/flex_test.cpp"],
    tags = ["manual"],  # built through flex_test, with real set to flex_real
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

real_test(
    name = "flex_test",
    size = "small",
    real = "flex_real",
    target = ":flex_test_on_flex_real",
)

cc_test(
    name = "core_test",
    size = "small",
    srcs = ["tests/core_test.cpp"],
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "ragdoll_test",
    size = "small",
    srcs = ["tests/ragdoll_test.cpp"],
    deps = [
        ":ragdoll_physics",
        "@gtest//:gtest_main",
    ],
)

# The ragdoll in fixed point, as the DS runs it (Q16.16), and in Q32.32
[real_test(
    name = "ragdoll_test_" + real,
    size = "medium",
    real = real,
    target = ":ragdoll_test",
) for real in [
    "flex_point",
    "flex_point16",
]]

cc_test(
    name = "collision_test",
    size = "small",
    srcs = ["tests/collision_test.cpp"],
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "forces_test",
    size = "small",
    srcs = ["tests/forces_test.cpp"],
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "contacts_test",
    size = "small",
    srcs = ["tests/contacts_test.cpp"],
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "random_test",
    size = "small",
    srcs = ["tests/random_test.cpp"],
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "impact_sounds_test",
    size = "small",
    srcs = ["tests/impact_sounds_test.cpp"],
    deps = [
        ":app",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "fixed_steps_test",
    size = "small",
    srcs = ["tests/fixed_steps_test.cpp"],
    deps = [
        ":fixed_steps",
        "@gtest//:gtest_main",
    ],
)

# Random scenes run with both resolvers, every body checked after every step
# (see the file)
cc_test(
    name = "stress_test",
    size = "medium",
    srcs = ["tests/stress_test.cpp"],
    deps = [
        ":cyclone",
        "@gtest//:gtest_main",
    ],
)

# The engine's sources and the demos' headers that need no window (the
# ragdoll, the physics clock, the knocks), for builds outside Bazel's C++
# rules: the DS app staged into devkitPro's Makefile (see //tools/devkitpro)
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/*.cpp"]) + [
        "src/demos/fixed_steps.h",
        "src/demos/impact_knocks.cpp",
        "src/demos/impact_knocks.h",
        "src/demos/ragdoll/ragdoll_physics.h",
    ],
    visibility = ["//visibility:public"],
)
