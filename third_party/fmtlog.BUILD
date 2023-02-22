load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "fmtlog",
    srcs = ["fmtlog-inl.h","fmtlog.cc"],
    hdrs = ["fmtlog.h"],
    visibility = ["//visibility:public"],
    deps = ["@fmt",],
    include_prefix = "fmtlog",
)