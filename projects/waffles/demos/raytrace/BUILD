load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# Reads RenderMan (.rib) scenes into Waffles' ray tracer
cc_library(
    name = "WRibParser",
    srcs = ["src/WRibParser.cpp"],
    hdrs = ["src/WRibParser.h"],
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
        ":WRibParser",
        "//projects/waffles:Gui",
        "//projects/waffles:WClasses",
    ],
)

cc_test(
    name = "WRibParser_test",
    size = "small",
    srcs = ["tests/WRibParser_test.cpp"],
    data = glob(["bin/*.rib"]),
    deps = [
        ":WRibParser",
        "@gtest//:gtest_main",
    ],
)
