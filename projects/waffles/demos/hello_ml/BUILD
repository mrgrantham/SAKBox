
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "hello_ml",
    srcs = ["src/main.cpp"],
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "@fmt",
        "@backward",
    ],
)

clang_format(name = "hello_ml")