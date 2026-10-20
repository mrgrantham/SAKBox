load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# The robot (a Bullet ragdoll), the policy network that moves it and the
# training that improves the policy, with Bullet's demo framework to show it
# in an SDL2 window
cc_library(
    name = "jumping_ragdoll",
    srcs = glob(
        ["src/*.cpp"],
        exclude = ["src/main.cpp"],
    ),
    hdrs = glob(["src/*.h"]),
    defines = ["GL_SILENCE_DEPRECATION"],  # macOS deprecates OpenGL but still provides it
    linkopts = select({
        "@platforms//os:macos": ["-framework OpenGL"],
        "//conditions:default": [
            "-lGL",
            "-lGLU",
        ],
    }) + ["-pthread"],
    strip_include_prefix = "src",
    deps = [
        "//projects/waffles:WClasses",
        "@bullet//:BulletDynamics",
        "@bullet//:BulletCollision",
        "@bullet//:LinearMath",
        "@sdl2",
    ],
)

cc_binary(
    name = "jumper",
    srcs = ["src/main.cpp"],
    deps = [":jumping_ragdoll"],
)

cc_test(
    name = "PolicyLearner_test",
    size = "medium",
    srcs = ["tests/PolicyLearner_test.cpp"],
    deps = [
        ":jumping_ragdoll",
        "@gtest//:gtest_main",
    ],
)
