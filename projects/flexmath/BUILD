load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "flexmath",
    srcs = ["src/FlexController.cpp","src/FlexGroup.cpp","src/FlexMatrix.cpp","src/FlexPoint.cpp","src/FlexVector.cpp"],
    includes = ["include/FlexController.hpp","include/FlexGroup.hpp","include/FlexMatrix.hpp","include/FlexPoint.hpp","include/FlexVector.hpp"],
    copts = ["-O2", "-fPIC"],
)