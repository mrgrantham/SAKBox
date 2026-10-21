load("@rules_cc//cc:defs.bzl", "cc_library")

# What the fourier_net and time_series demos share: both fit a network to a
# time series.

# Identity, perturbation, regularization and input renormalization for a
# WBlockLinear's weights (what Waffles' old WLayerClassic could do)
cc_library(
    name = "LinearWeights",
    hdrs = ["LinearWeights.h"],
    include_prefix = "series_networks",
    visibility = ["//projects/waffles/demos:__subpackages__"],
    deps = ["//projects/waffles:WClasses"],
)

# Draws a network's curve over a time series, with the data, as an SVG
cc_library(
    name = "SeriesPlot",
    srcs = ["SeriesPlot.cpp"],
    hdrs = ["SeriesPlot.h"],
    include_prefix = "series_networks",
    visibility = ["//projects/waffles/demos:__subpackages__"],
    deps = ["//projects/waffles:WClasses"],
)
