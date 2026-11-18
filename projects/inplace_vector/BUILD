load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A list with its room inside it (Inplace::Vector, C++26's
# std::inplace_vector in miniature): no allocation, copied as one block.
# Header-only, no dependencies. See README.md.
cc_library(
    name = "inplace_vector",
    hdrs = ["include/Inplace/Vector.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

cc_test(
    name = "Vector_test",
    size = "small",
    srcs = ["tests/Vector_test.cpp"],
    deps = [
        ":inplace_vector",
        "@gtest//:gtest_main",
    ],
)

# The header, for the consoles' Makefile builds (add inplace_vector/include
# to the Makefile's INCLUDES)
filegroup(
    name = "sources",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"],
)
