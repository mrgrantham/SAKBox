load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# What the padds send each other, and the window's blending of snapshots (no
# Cyclone, GL or ImGui)
cc_library(
    name = "ragdoll_messages",
    hdrs = [
        "src/RagdollMessages.hpp",
        "src/SnapshotTimeline.hpp",
    ],
    strip_include_prefix = "src",
    deps = ["@glm"],
)

# The physics padd: Cyclone's ragdoll on its own thread (no window)
cc_library(
    name = "ragdoll_physics_padd",
    hdrs = ["src/RagdollPhysicsPadd.hpp"],
    strip_include_prefix = "src",
    deps = [
        ":ragdoll_messages",
        "//projects/cyclone",
        "//projects/cyclone:fixed_steps",
        "//projects/cyclone:impact_knocks",
        "//projects/cyclone:ragdoll_physics",
        "//projects/scratchpadd/audio:audio_made_sounds",
        "//projects/scratchpadd/audio:audio_messages",
        "//projects/scratchpadd/basis",
    ],
)

cc_binary(
    name = "cyclone_padds",
    srcs = [
        "src/RagdollWindowPadd.cpp",
        "src/RagdollWindowPadd.hpp",
        "src/main.cpp",
    ],
    deps = [
        ":ragdoll_messages",
        ":ragdoll_physics_padd",
        "//projects/scratchpadd/audio",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/gl",
        "@glm",
        "@imgui",
    ],
)

cc_test(
    name = "SnapshotTimeline_test",
    size = "small",
    srcs = ["tests/SnapshotTimeline_test.cpp"],
    deps = [
        ":ragdoll_messages",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "RagdollPhysicsPadd_test",
    size = "small",
    srcs = ["tests/RagdollPhysicsPadd_test.cpp"],
    deps = [
        ":ragdoll_physics_padd",
        "@gtest//:gtest_main",
    ],
)
