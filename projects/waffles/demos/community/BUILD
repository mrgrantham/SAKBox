load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "community",
    srcs = glob([
        "src/*.cpp",
        "src/*.h",
    ]),
    linkopts = ["-pthread"],
    deps = ["//projects/waffles:WClasses"],
)
