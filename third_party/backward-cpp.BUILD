load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

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
)