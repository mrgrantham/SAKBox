load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/windows:defs.bzl", "NOT_ON_WINDOWS")

cc_binary(
    name = "exampleBacktrace",
    srcs = ["src/exampleBacktrace.cpp"],
    # Calls backtrace() from execinfo.h (Linux and macOS) itself, beside
    # backward-cpp, so it isn't built on Windows
    target_compatible_with = NOT_ON_WINDOWS,
    deps = [
        "@backward",
        "@fmt",
    ],
)
