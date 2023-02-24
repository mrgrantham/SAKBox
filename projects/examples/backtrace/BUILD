load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "exampleBacktrace",
    srcs = ["src/exampleBacktrace.cpp"],
    deps = [
        "@backward",
        "@fmt",
    ],
)
clang_format(name = "backtrace")