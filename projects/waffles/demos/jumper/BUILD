load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# The robot (simulated by Cyclone), the policy network that moves it and the
# training that improves the policy: no window, so training and the tests
# run anywhere
cc_library(
    name = "jumping_robot",
    srcs = [
        "src/PolicyLearner.cpp",
        "src/Robot.cpp",
    ],
    hdrs = [
        "src/PolicyLearner.h",
        "src/Robot.h",
    ],
    strip_include_prefix = "src",
    deps = [
        "//projects/cyclone",
        "//projects/waffles:WClasses",
    ],
)

# Training and scoring from the command line, and the robot jumping in a
# window on the Cyclone demos' framework
cc_binary(
    name = "jumper",
    srcs = [
        "src/JumperDemo.cpp",
        "src/JumperDemo.h",
        "src/main.cpp",
    ],
    deps = [
        ":jumping_robot",
        "//projects/cyclone:app",
    ],
)

cc_test(
    name = "PolicyLearner_test",
    size = "medium",
    srcs = ["tests/PolicyLearner_test.cpp"],
    deps = [
        ":jumping_robot",
        "@gtest//:gtest_main",
    ],
)
