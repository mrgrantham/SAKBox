
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_binary(
    name = "examplePng",
    srcs = glob(["src/pngtest.cpp"]),
    data = glob(["data/**"]),
    deps = [
        "@fmt",
        "@libpng",
    ],
)
