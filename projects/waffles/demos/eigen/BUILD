
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_binary(
    name = "eigen",
    srcs = ["src/main.cpp"],
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "@eigen",
    ],
)
