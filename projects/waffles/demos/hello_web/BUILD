load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "hello_web",
    srcs = glob(["src/*.cpp", "src/*.h"], allow_empty = True),
    data = glob(["bin/**"]),
    linkopts = ["-pthread"],
    deps = ["//projects/waffles:WClasses"],
)
