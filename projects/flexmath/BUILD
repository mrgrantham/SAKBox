load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

cc_library(
    name = "flexmath",
    srcs = [
        "src/FlexController.cpp",
        "src/FlexGroup.cpp",
        "src/FlexMatrix.cpp",
        "src/FlexPoint.cpp",
        "src/FlexVector.cpp",
    ],
    hdrs = [
        "include/FlexController.hpp",
        "include/FlexGroup.hpp",
        "include/FlexMatrix.hpp",
        "include/FlexPoint.hpp",
        "include/FlexPrecision.hpp",
        "include/FlexReal.hpp",
        "include/FlexVector.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["@fmt"],
)

# The examples from README.md and the header comments
cc_test(
    name = "Examples_test",
    srcs = ["tests/Examples_test.cpp"],
    deps = [
        ":flexmath",
        "@catch2//:catch2_main",
    ],
)

cc_test(
    name = "FlexGroup_test",
    srcs = ["tests/FlexGroup_test.cpp"],
    deps = [
        ":flexmath",
        "@catch2//:catch2_main",
    ],
)

cc_test(
    name = "FlexMatrix_test",
    srcs = ["tests/FlexMatrix_test.cpp"],
    deps = [
        ":flexmath",
        "@catch2//:catch2_main",
    ],
)

cc_test(
    name = "FlexPoint_test",
    srcs = ["tests/FlexPoint_test.cpp"],
    deps = [
        ":flexmath",
        "@gtest",
    ],
)

cc_test(
    name = "FlexReal_test",
    srcs = ["tests/FlexReal_test.cpp"],
    deps = [
        ":flexmath",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Rounding_test",
    srcs = ["tests/Rounding_test.cpp"],
    deps = [
        ":flexmath",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "FlexVector_test",
    srcs = ["tests/FlexVector_test.cpp"],
    deps = [
        ":flexmath",
        "@catch2//:catch2_main",
    ],
)

clang_format(name = "flexmath")
