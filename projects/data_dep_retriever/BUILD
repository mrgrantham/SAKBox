load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "data_dep_retriever",
    hdrs = glob(["include/DataDepRetriever/*.hpp"]),
    srcs = ["src/DataDependencies.cpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "@bazel_tools//tools/cpp/runfiles",
        "@spdlog",
    ],
)