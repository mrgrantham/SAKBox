load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# The device profile: what this device is and can do, and how an app should
# run on it (see README.md)
cc_library(
    name = "device_profile",
    srcs = [
        "src/CurrentProfile.cpp",
        "src/DeviceProfile.cpp",
    ],
    hdrs = ["include/ScratchPadd/DeviceProfile/DeviceProfile.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        "//projects/scratchpadd/input",
        "//projects/scratchpadd/storage",
    ],
)

# DeviceProfilePadd: the profile as shared state, and a debug build's tab
cc_library(
    name = "device_profile_padd",
    hdrs = ["include/ScratchPadd/DeviceProfile/DeviceProfilePadd.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":device_profile",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/storage",
        "//projects/scratchpadd/storage:storage_padd",  # (SaveFile)
    ],
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = ["//visibility:public"],
)

cc_test(
    name = "DeviceProfile_test",
    size = "small",
    srcs = ["tests/DeviceProfile_test.cpp"],
    deps = [
        ":device_profile",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "DeviceProfilePadd_test",
    size = "small",
    srcs = ["tests/DeviceProfilePadd_test.cpp"],
    deps = [
        ":device_profile_padd",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
    ],
)
