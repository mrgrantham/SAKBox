load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# Reads RenderMan (.rib) scenes into Waffles' ray tracer
cc_library(
    name = "GRibParser",
    srcs = ["src/GRibParser.cpp"],
    hdrs = ["src/GRibParser.h"],
    strip_include_prefix = "src",
    deps = ["//projects/waffles:WClasses"],
)

cc_binary(
    name = "raytrace",
    srcs = [
        "src/RayTrace.cpp",
        "src/RayTrace.h",
        "src/main.cpp",
    ],
    data = glob(["bin/*.rib"]),
    linkopts = ["-pthread"],
    deps = [
        ":GRibParser",
        "//projects/waffles:Gui",
        "//projects/waffles:WClasses",
    ],
)

cc_test(
    name = "GRibParser_test",
    size = "small",
    srcs = ["tests/GRibParser_test.cpp"],
    data = glob(["bin/*.rib"]),
    deps = [
        ":GRibParser",
        "@gtest//:gtest_main",
    ],
)
