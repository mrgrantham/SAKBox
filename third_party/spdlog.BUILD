load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

# Do not use in projects with fmt library. They will conflict.
# When new version of spdlog is released check to see if this
# conflict error has been resolved

cc_library(
    name = "spdlog",
    hdrs = glob([
        "include/**/*.cc",
        "include/**/*.h",
    ]),
    includes = ["include"],
    visibility = ["//visibility:public"],
)