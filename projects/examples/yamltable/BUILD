load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

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
