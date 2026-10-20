
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_binary(
    name = "hello_gui",
    srcs = ["src/main.cpp"],
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "//projects/waffles:Gui",
    ],
)
