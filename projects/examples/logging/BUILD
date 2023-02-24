load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_library(
    name = "hellolib",
    srcs = ["src/hello.cpp"],
    hdrs = ["include/hello.hpp"],
    strip_include_prefix = "include/",
)

cc_binary(
    name = "greet",
    srcs = ["src/hello_main.cpp"],
    deps = [
        ":hellolib",
        "@fmt",
    ],
)

cc_binary(
    name = "spd_example",
    srcs = ["src/spd_example_main.cpp"],
    deps = [
        "@spdlog",
    ],
)

clang_format(name = "logging")