load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "hellolib",
    srcs = ["src/hello.cpp"],
    includes = ["include/hello.h"],
    strip_include_prefix = "include/",
)

cc_binary(
    name = "greet",
    srcs = ["src/main.cpp"],
    deps = [
        ":hellolib",
        "@fmt",
    ],
)
