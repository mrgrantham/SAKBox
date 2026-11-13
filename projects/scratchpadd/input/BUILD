load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test", "objc_library")

# The controller, the same on every platform (Gamepad.hpp), read by each
# platform's own API: GameController on a Mac, XInput on Windows, evdev on
# Linux (and hid on a 3DS, the keys on a DS, from :sources). See README.md.
cc_library(
    name = "input",
    srcs = [
        "src/Devices.cpp",
        "src/Gamepad.cpp",
        "src/Keyboard.cpp",
        "src/Reader.cpp",
    ] + select({
        "@platforms//os:windows": ["src/PlatformWindows.cpp"],
        "@platforms//os:linux": ["src/PlatformLinux.cpp"],
        "//conditions:default": [],
    }),
    hdrs = [
        "include/ScratchPadd/Input/Devices.hpp",
        "include/ScratchPadd/Input/Gamepad.hpp",
        "include/ScratchPadd/Input/Keyboard.hpp",
        "include/ScratchPadd/Input/Reader.hpp",
    ],
    linkopts = select({
        "@platforms//os:windows": ["-DEFAULTLIB:xinput.lib"],
        "//conditions:default": [],
    }),
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = select({
        "@platforms//os:macos": [":platform_mac"],
        "//conditions:default": [],
    }),
)

# The Mac's reader: the input library's only Objective-C
objc_library(
    name = "platform_mac",
    srcs = ["src/PlatformMac.mm"],
    hdrs = [
        "include/ScratchPadd/Input/Devices.hpp",
        "include/ScratchPadd/Input/Gamepad.hpp",
    ],
    copts = ["-std=c++20"],
    includes = ["include"],
    sdk_frameworks = [
        "CoreFoundation",
        "Foundation",
        "GameController",
    ],
    target_compatible_with = ["@platforms//os:macos"],
)

# GamePadd: the pad as shared state, for apps made of padds
cc_library(
    name = "game_padd",
    hdrs = ["include/ScratchPadd/Input/GamePadd.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":input",
        "//projects/scratchpadd/basis",
    ],
)

cc_test(
    name = "Input_test",
    size = "small",
    srcs = ["tests/Input_test.cpp"],
    deps = [
        ":input",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "GamePadd_test",
    size = "small",
    srcs = ["tests/GamePadd_test.cpp"],
    deps = [
        ":game_padd",
        "@gtest//:gtest_main",
    ],
)

# The sources, for the 3DS's and DS's Makefile builds (each platform file
# compiles to nothing elsewhere; the Mac's Objective-C is left out)
filegroup(
    name = "sources",
    srcs = glob(
        ["include/**", "src/**"],
        exclude = ["src/PlatformMac.mm"],
    ),
    visibility = ["//visibility:public"],
)

# Prints the controller as it's read, for checking a platform's reader with
# a real pad
cc_binary(
    name = "show_pad",
    srcs = ["tools/ShowPad.cpp"],
    deps = [":input"],
)
