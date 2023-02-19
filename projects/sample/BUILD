load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "hellolib",
    srcs = ["hello.cpp"],
    hdrs = ["hello.h"],
)

cc_binary(
    name = "greet",
    srcs = ["main.cpp"],
    deps = [
        ":hellolib",
        "@fmt",
    ],
)