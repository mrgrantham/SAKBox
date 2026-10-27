load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# The fixed-point types without fmt, for a DS build (devkitPro's Makefile
# builds, given these by extra_srcs): FixedPoint.hpp, what it includes, and
# its DS arithmetic
filegroup(
    name = "fixed_point_sources",
    srcs = [
        "include/FixedPoint.hpp",
        "include/FixedPointNds.hpp",
        "include/FlexPrecision.hpp",
    ],
    visibility = ["//visibility:public"],
)

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
        "src/FlexVector.cpp",
    ],
    hdrs = [
        "include/FixedPoint.hpp",
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

# FlexPoint16 and FlexPoint's arithmetic, maths functions and the portable
# 128-bit code
cc_test(
    name = "FixedPoint_test",
    srcs = ["tests/FixedPoint_test.cpp"],
    deps = [
        ":flexmath",
        "@gtest//:gtest_main",
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
