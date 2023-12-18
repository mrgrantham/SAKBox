load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "utilities",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = ["src/Logger.cpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "@fmt",
        "@spdlog",
    ],
)