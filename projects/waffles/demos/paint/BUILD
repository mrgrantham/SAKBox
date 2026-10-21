load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "paint",
    srcs = glob(["src/*.cpp","src/*.h"]),
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "//projects/waffles:Gui",
        "//projects/waffles:WImagePng",
        "@libjpeg_turbo//:jpeg",
    ],
)
