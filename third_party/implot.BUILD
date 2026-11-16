load("@rules_cc//cc:defs.bzl", "cc_library")

# ImPlot: plots for ImGui (its demo left out)
cc_library(
    name = "implot",
    srcs = [
        "implot.cpp",
        "implot_items.cpp",
    ],
    hdrs = [
        "implot.h",
        "implot_internal.h",
    ],
    visibility = ["//visibility:public"],
    deps = ["@imgui"],
)
