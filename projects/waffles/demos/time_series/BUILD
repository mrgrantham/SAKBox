load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "time_series",
    srcs = ["src/main.cpp"],
    data = [
        "bin/test.arff",
        "bin/train.arff",
    ],
    linkopts = ["-pthread"],
    deps = [
        "//projects/waffles:WClasses",
        "//projects/waffles/demos/series_networks:LinearWeights",
        "//projects/waffles/demos/series_networks:SeriesPlot",
    ],
)
