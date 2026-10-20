load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "hello_console",
    srcs = glob(["src/*.cpp", "src/*.h"], allow_empty = True),
    linkopts = ["-pthread"],
    deps = ["//projects/waffles:WClasses"],
)
