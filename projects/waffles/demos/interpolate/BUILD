load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "interpolate",
    srcs = ["src/main.cpp"],
    data = glob(["bin/**"]),
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "//projects/waffles:Gui",
        "//projects/waffles:WImagePng",
    ],
)
