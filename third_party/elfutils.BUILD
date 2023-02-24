load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "source",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

configure_make(
    name = "elfutils",
    configure_in_place = True,
    lib_source = ":source",
    targets = [
        "simple",
        "install",
    ],
    deps = [":built_with_bazel"],
)