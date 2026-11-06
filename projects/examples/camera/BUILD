load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "exampleCamera",
    srcs = ["src/exampleCamera.cpp"],
    deps = [
        "@fmt",
        # Only the OpenCV modules it uses: the core and capturing video
        "@opencv//:core",
        "@opencv//:videoio",
        "@sdl2",
    ],
)
