load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

# Cosmic Hex's balance lab, step 1 (see README.md): a batch's settings, bot
# games played eight at once, and the results as charts while they come in.
#   bazel run -c opt //projects/cosmic_hex/balance_lab
cc_binary(
    name = "balance_lab",
    srcs = [
        "src/LabWindowPadd.cpp",
        "src/LabWindowPadd.hpp",
        "src/SimulationPadd.hpp",
        "src/Tally.hpp",
        "src/main.cpp",
    ],
    data = ["//projects/cosmic_hex/layouts:layouts.txt"],
    deps = [
        "//projects/cosmic_hex/simulate:batch",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/debug_log",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop",
        "@imgui",
        "@implot",
    ],
)

cc_test(
    name = "Tally_test",
    size = "small",
    srcs = [
        "src/Tally.hpp",
        "tests/Tally_test.cpp",
    ],
    deps = [
        "//projects/cosmic_hex/simulate:batch",
        "@gtest//:gtest_main",
    ],
)
