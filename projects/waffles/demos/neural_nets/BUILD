load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "neural_nets",
    srcs = glob(["src/*.cpp", "src/*.h"], allow_empty = True),
    data = glob(["*.arff"]),
    linkopts = ["-pthread"],
    deps = ["//projects/waffles:WClasses"],
)
