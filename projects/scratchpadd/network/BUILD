load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# An app's networking (see README.md): sockets the same everywhere (POSIX,
# Winsock, the 3DS's soc), the I/O loop that runs services, and the first
# services: text streamed out (the log) and lines both ways (remote
# controls). Plain C++, no padds; tested on desktops. Not on a DS.
cc_library(
    name = "network",
    srcs = [
        "src/Loop.cpp",
        "src/Services.cpp",
        "src/Sockets.cpp",
    ],
    hdrs = [
        "include/ScratchPadd/Network/LineService.hpp",
        "include/ScratchPadd/Network/Loop.hpp",
        "include/ScratchPadd/Network/Service.hpp",
        "include/ScratchPadd/Network/Sockets.hpp",
        "include/ScratchPadd/Network/StreamService.hpp",
    ],
    linkopts = select({
        "@platforms//os:windows": ["ws2_32.lib"],
        "//conditions:default": [],
    }),
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# The padds: NetworkPadd (runs the services registered with it) and
# RemoteControlsPadd (an app's controls from another device, on port 5006)
cc_library(
    name = "network_padd",
    hdrs = [
        "include/ScratchPadd/Network/NetworkPadd.hpp",
        "include/ScratchPadd/Network/RemoteControlsPadd.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":network",
        "//projects/scratchpadd/basis",
    ],
)

cc_test(
    name = "RemoteControls_test",
    size = "small",
    srcs = ["tests/RemoteControls_test.cpp"],
    deps = [
        ":network_padd",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Loop_test",
    size = "small",
    srcs = ["tests/Loop_test.cpp"],
    deps = [
        ":network",
        "@gtest//:gtest_main",
    ],
)

# For the 3DS's Makefile builds (devkitpro_3ds_app's extra_srcs, with
# FOLDER/src in the Makefile's SOURCES)
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = ["//visibility:public"],
)
