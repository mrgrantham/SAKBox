load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "audio",
    hdrs = glob(["include/**/*.hpp"]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "include/",
    deps = [
        "//projects/scratchpadd/basis",
    ],
)