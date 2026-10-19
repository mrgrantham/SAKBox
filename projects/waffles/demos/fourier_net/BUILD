
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_binary(
    name = "fourier_net",
    srcs = glob(["src/*.cpp","src/*.h"]),
    # The NASDAQ series main.cpp trains and tests on, restored from upstream
    # history (removed there in b085347 with other demo outputs).
    data = ["data/train.arff", "data/test.arff"],
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "@fmt",
    ],
)
