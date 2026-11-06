load("@rules_cc//cc:defs.bzl", "cc_library")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

cc_library(
    name = "backward",
    hdrs = ["backward.hpp"],
    includes = ["."],
    visibility = ["//visibility:public"],
    include_prefix = "backward",
    # deps = ["@binutils"],
    # defines = ["BACKWARD_HAS_BFD=1"],
)