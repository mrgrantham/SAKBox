
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "hello_gui",
    srcs = glob(["src/*.cpp","src/*.h"]),
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "//projects/waffles:gui",
    ],
)

clang_format(name = "hello_gui")