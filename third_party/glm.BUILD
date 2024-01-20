load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "glm",
    hdrs = glob(["glm/**/*"]),
    visibility = ["//visibility:public"],
    # These next two lines allows use of include brackets
    # for this lib
    strip_include_prefix = "glm",
    include_prefix = "glm",
)