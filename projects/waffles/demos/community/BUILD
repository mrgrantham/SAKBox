load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_binary(
    name = "community",
    srcs = glob([
        "src/*.cpp",
        "src/*.h",
    ]),
    linkopts = ["-pthread"],
    deps = ["//projects/waffles:WClasses"],
)

clang_format(name = "community")
