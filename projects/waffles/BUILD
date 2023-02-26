
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_library(
    name = "WClasses",
    srcs = glob(["src/WClasses/*.cpp",
    ]),
    hdrs = glob([
        "src/WClasses/*.h",
    ]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "src",
)

cc_library(
    name = "gui",
    srcs = glob(["src/gui/*.cpp",
    ]),
    hdrs = glob([
        "src/gui/*.h",
    ]),
    deps = ["@sdl2","//projects/waffles:WClasses","@fmt"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "src",
)

cc_library(
    name = "png",
    srcs = glob(["src/png/*.cpp",
    ]),
    hdrs = glob([
        "src/png/*.h",
    ]),
    deps = ["@libpng","//projects/waffles:WClasses",
],
    visibility = ["//visibility:public"],
    strip_include_prefix = "src",
)

cc_binary(
    name = "audio",
    srcs = ["src/audio/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "cluster",
    srcs = ["src/cluster/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "dimred",
    srcs = ["src/dimred/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "learn",
    srcs = ["src/learn/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "plot",
    srcs = ["src/plot/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "recommend",
    srcs = ["src/recommend/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "sparse",
    srcs = ["src/sparse/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "test",
    srcs = ["src/test/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "transform",
    srcs = ["src/transform/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "wizard",
    srcs = ["src/wizard/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

clang_format(name = "waffles")