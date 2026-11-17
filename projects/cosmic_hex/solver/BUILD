load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# The layout solver (LayoutSolver.hpp): boards made to order, ranked by how
# equal every home's access is. Plain C++; the balance lab gives it a panel,
# and solve_layouts runs it from the command line.
cc_library(
    name = "solver",
    srcs = ["src/LayoutSolver.cpp"],
    hdrs = ["include/CosmicHex/Solver/LayoutSolver.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        "//projects/cosmic_hex/rules",
        "//projects/hex_grid",
        "//projects/seeded_random",
    ],
)

cc_binary(
    name = "solve_layouts",
    srcs = ["src/solve_layouts.cpp"],
    deps = [":solver"],
)

cc_test(
    name = "LayoutSolver_test",
    size = "small",
    srcs = ["tests/LayoutSolver_test.cpp"],
    deps = [
        ":solver",
        "@gtest//:gtest_main",
    ],
)
