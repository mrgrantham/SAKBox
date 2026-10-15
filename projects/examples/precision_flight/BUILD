load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/clang_format:clang_format.bzl", "clang_format")

# The simulation: no graphics, input or ScratchPadd dependency
cc_library(
    name = "flight_sim",
    srcs = ["src/FlightSim.cpp"],
    hdrs = [
        "src/FlightMessages.hpp",
        "src/FlightSim.hpp",
    ],
    strip_include_prefix = "src",
    deps = [
        "//projects/flexmath",
        "@glm",
    ],
)

# The padds that run the simulation and log its events (no graphics)
cc_library(
    name = "flight_padds",
    hdrs = [
        "src/EventLogPadd.hpp",
        "src/FlightSimPadd.hpp",
    ],
    strip_include_prefix = "src",
    deps = [
        ":flight_sim",
        "//projects/scratchpadd/basis",
    ],
)

cc_binary(
    name = "precision_flight",
    srcs = [
        "src/FlightDisplayPadd.cpp",
        "src/FlightDisplayPadd.hpp",
        "src/main.cpp",
    ],
    deps = [
        ":flight_padds",
        "//projects/scratchpadd/graphics",
        "@glew",
        "@glfw",
        "@glm",
        "@imgui",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_backend_opengl3",
    ],
)

cc_test(
    name = "FlightSim_test",
    size = "small",
    srcs = ["tests/FlightSim_test.cpp"],
    data = glob(["tests/data/*.txt"]),
    deps = [
        ":flight_sim",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "FlightPadds_test",
    size = "small",
    srcs = ["tests/FlightPadds_test.cpp"],
    data = glob(["tests/data/*.txt"]),
    deps = [
        ":flight_padds",
        "@gtest//:gtest_main",
    ],
)

clang_format(name = "precision_flight")
