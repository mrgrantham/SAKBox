load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# Maths functions that give the same result on every platform (CORE-MATH's,
# correctly rounded; see CorrectlyRounded.hpp)
cc_library(
    name = "correctly_rounded",
    hdrs = ["include/CorrectlyRounded.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["@core_math"],
)

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
    deps = [
        ":correctly_rounded",
        "@fmt",
    ],
)

# CORE-MATH's functions give the correctly rounded result where the
# system's may not
cc_test(
    name = "CorrectlyRounded_test",
    srcs = ["tests/CorrectlyRounded_test.cpp"],
    deps = [
        ":correctly_rounded",
        "@gtest//:gtest_main",
    ],
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
