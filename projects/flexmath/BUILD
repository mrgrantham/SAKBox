load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "flexmath",
    srcs = ["src/FlexController.cpp","src/FlexGroup.cpp","src/FlexMatrix.cpp","src/FlexPoint.cpp","src/FlexVector.cpp"],
    hdrs = ["include/FlexController.hpp","include/FlexGroup.hpp","include/FlexMatrix.hpp","include/FlexPoint.hpp","include/FlexVector.hpp"],
    strip_include_prefix = "include/",
    copts = ["-O2", "-fPIC"],
    deps = ["@fmt"],
)

cc_test(
    name = "test_FlexGroup",
    srcs = ["tests/test_FlexGroup.cpp"],
    deps = [":flexmath","@catch2"],
)

cc_test(
    name = "test_FlexMatrix",
    srcs = ["tests/test_FlexMatrix.cpp"],
    deps = [":flexmath","@catch2"],
)

cc_test(
    name = "test_FlexPoint",
    srcs = ["tests/test_FlexPoint.cpp"],
    deps = [":flexmath","@catch2"],
)

cc_test(
    name = "test_FlexVector",
    srcs = ["tests/test_FlexVector.cpp"],
    deps = [":flexmath","@catch2"],
)