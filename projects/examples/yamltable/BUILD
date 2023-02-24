load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "exampleYaml",
    srcs = ["src/exampleYaml.cpp"],
    data = ["data/data.yaml"],
    deps = [
        "@variadic_table",
        "@rapidyaml",
        "@yamlcpp",
        "@fmt",
    ],
)
clang_format(name = "yamltable")