
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

data_src = glob(["caronhill/data/**"])
print(data_src)
cc_binary(
    name = "caronhill",
    srcs = glob(["src/*.cpp","src/*.h"]),
    data = glob(["data/**"]),
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "@libpng",
        "@sdl2",
        "@fmt",
    ],
)

clang_format(name = "demos")