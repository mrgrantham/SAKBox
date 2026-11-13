load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# A debug build's log off the device (see README.md):
# - :log_server, a log kept in memory (LogBuffer) and served over the
#   network (LogServer), plain C++ for any program;
# - :debug_log, the Session an app opens: every ScratchPadd logger served
#   over Wi-Fi from a 3DS (read by tools/console_log), written to the SD card
#   on a DS, the terminal on desktops.
cc_library(
    name = "log_server",
    srcs = ["src/server/LogBuffer.cpp"] + select({
        "@platforms//os:windows": [],  # (no POSIX sockets: LogBuffer only)
        "//conditions:default": ["src/server/LogServer.cpp"],
    }),
    hdrs = [
        "include/ScratchPadd/DebugLog/LogBuffer.hpp",
        "include/ScratchPadd/DebugLog/LogServer.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "debug_log",
    srcs = [
        "src/session/CardLog.cpp",
        "src/session/Session.cpp",
    ],
    hdrs = [
        "include/ScratchPadd/DebugLog/CardLog.hpp",
        "include/ScratchPadd/DebugLog/LogBufferSink.hpp",
        "include/ScratchPadd/DebugLog/Session.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":log_server",
        "//projects/scratchpadd/utilities",
        "@spdlog",
    ],
)

# The padd that writes a DS's debug log (CardLog) when nothing else has work
cc_library(
    name = "writer_padd",
    hdrs = ["include/ScratchPadd/DebugLog/WriterPadd.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":debug_log",
        "//projects/scratchpadd/basis",
    ],
)

cc_test(
    name = "CardLog_test",
    size = "small",
    srcs = ["tests/CardLog_test.cpp"],
    deps = [
        ":debug_log",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "WriterPadd_test",
    size = "small",
    srcs = ["tests/WriterPadd_test.cpp"],
    deps = [
        ":writer_padd",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "LogBuffer_test",
    size = "small",
    srcs = ["tests/LogBuffer_test.cpp"],
    deps = [
        ":debug_log",
        "@gtest//:gtest_main",
    ],
)

# The server on this machine's localhost (not Windows: no POSIX sockets)
cc_test(
    name = "LogServer_test",
    size = "small",
    srcs = ["tests/LogServer_test.cpp"],
    target_compatible_with = select({
        "@platforms//os:windows": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
    deps = [
        ":log_server",
        "@gtest//:gtest_main",
    ],
)

# For the 3DS's and DS's Makefile builds (devkitpro_3ds_app's and
# devkitpro_nds_app's extra_srcs): everything (with the Makefile's SOURCES
# taking FOLDER/src/server and FOLDER/src/session), or the server alone
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "server_sources",
    srcs = [
        "include/ScratchPadd/DebugLog/LogBuffer.hpp",
        "include/ScratchPadd/DebugLog/LogServer.hpp",
    ] + glob(["src/server/**"]),
    visibility = ["//visibility:public"],
)
