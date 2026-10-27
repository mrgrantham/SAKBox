load("@rules_cc//cc:defs.bzl", "cc_library")

# CORE-MATH (https://core-math.gitlabpages.inria.fr/): correctly rounded
# maths functions, one C file each. Only the double-precision ones the repo
# uses are built; //projects/flexmath:correctly_rounded declares them
# (cr_sin, cr_pow, ...), as CORE-MATH has no header of its own.
FUNCTIONS = [
    "acos",
    "asin",
    "atan2",
    "cos",
    "exp",
    "log",
    "pow",
    "sin",
]

cc_library(
    name = "core_math",
    srcs = ["src/binary64/%s/%s.c" % (f, f) for f in FUNCTIONS] + [
        # Their own headers
        "src/binary64/atan2/tint.h",
        "src/binary64/log/dint.h",
        "src/binary64/pow/dint.h",
        "src/binary64/pow/pow.h",
        "src/binary64/pow/qint.h",
    ],
    copts = ["-w"],  # third-party code, kept as published
    visibility = ["//visibility:public"],
)
