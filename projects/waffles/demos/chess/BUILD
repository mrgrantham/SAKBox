
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_binary(
    name = "chess",
    srcs = glob(["src/*.cpp","src/*.h"]),
    data = glob(["data/**"]),
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "//projects/waffles:Gui",
        "//projects/waffles:GImagePng",
        "@fmt",
        "@backward",
    ],
)
