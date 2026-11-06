load("@rules_cc//cc:defs.bzl", "cc_library")

# Windows builds with clang-cl, Clang's MSVC-compatible driver: Bazel picks
# a C++ toolchain by the platform it runs on, and its clang-cl toolchains
# want one marked clang-cl (.bazelrc's clang_cl config names this one).
platform(
    name = "clang_cl",
    constraint_values = ["@rules_cc//cc/private/toolchain:clang-cl"],
    parents = ["@platforms//host"],
)

config_setting(
    name = "arm64",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:arm64",
    ],
)

config_setting(
    name = "x86_64",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
)

# Clang's helpers for what the processor has no instruction for, such as
# dividing 128-bit integers (__divti3), from its runtime library
# (compiler-rt). Clang's own driver links them; on Windows, Bazel links with
# lld-link, which doesn't, so code that needs them depends on this. (The
# toolchain puts Clang's library folder on the linker's path.) Nothing
# elsewhere.
cc_library(
    name = "clang_builtins",
    linkopts = select({
        ":arm64": ["clang_rt.builtins-aarch64.lib"],
        ":x86_64": ["clang_rt.builtins-x86_64.lib"],
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
)
