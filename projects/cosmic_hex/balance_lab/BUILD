load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

# Cosmic Hex's balance lab (see README.md): a batch's settings, bot games
# played eight at once, the results as charts while they come in, and any
# game watched again, step by step.
#   bazel run -c opt //projects/cosmic_hex/balance_lab
cc_binary(
    name = "balance_lab",
    srcs = [
        "src/BoardDrawing.hpp",
        "src/BoardProving.cpp",
        "src/BoardProving.hpp",
        "src/GameReplay.hpp",
        "src/LabLayouts.hpp",
        "src/LabWindowPadd.cpp",
        "src/LabWindowPadd.hpp",
        "src/ReplayView.cpp",
        "src/ReplayView.hpp",
        "src/SimulationPadd.hpp",
        "src/Tally.hpp",
        "src/main.cpp",
    ],
    data = ["//projects/cosmic_hex/layouts:layouts.txt"],
    deps = [
        "//projects/action_log",
        "//projects/cosmic_hex/simulate:batch",
        # (new and delete through it: the bots' frame arena takes effect)
        "//projects/scratchpadd/memory_stats:counting_allocator",
        "//projects/cosmic_hex/solver",
        "//projects/hex_grid",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/debug_log",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop",
        "@imgui",
        "@imgui//:imgui_stdlib",
        "@implot",
    ],
)

cc_test(
    name = "LabLayouts_test",
    size = "small",
    srcs = [
        "src/LabLayouts.hpp",
        "tests/LabLayouts_test.cpp",
    ],
    data = ["//projects/cosmic_hex/layouts:layouts.txt"],
    deps = [
        "//projects/cosmic_hex/simulate:batch",
        "//projects/hex_grid",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "BoardProving_test",
    size = "small",
    srcs = [
        "src/BoardProving.cpp",
        "src/BoardProving.hpp",
        "tests/BoardProving_test.cpp",
    ],
    deps = [
        "//projects/cosmic_hex/simulate:batch",
        "@gtest//:gtest_main",
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

cc_test(
    name = "GameReplay_test",
    size = "small",
    srcs = [
        "src/GameReplay.hpp",
        "tests/GameReplay_test.cpp",
    ],
    data = ["//projects/cosmic_hex/layouts:layouts.txt"],
    deps = [
        "//projects/action_log",
        "//projects/cosmic_hex/simulate:batch",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "AgreedRules_test",
    size = "small",
    srcs = ["tests/AgreedRules_test.cpp"],
    data = ["//projects/cosmic_hex/layouts:layouts.txt"],
    deps = [
        "//projects/cosmic_hex/simulate:batch",
        "@gtest//:gtest_main",
    ],
)
