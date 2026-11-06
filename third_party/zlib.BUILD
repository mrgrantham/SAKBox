load("@rules_cc//cc:defs.bzl", "cc_library", "objc_library")
load("@//tools/windows:defs.bzl", "NOT_ON_WINDOWS")
load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Shows a standard library
cmake(
    name = "zlib",
    # rules_foreign_cc's builds hang on Windows on ARM (its Ninja wrapper
    # never returns), so this, and what uses it, isn't built on Windows
    target_compatible_with = NOT_ON_WINDOWS,
    lib_source = ":sources",
    # pass include/version123 to C/C++ provider as include directory
    # out_include_dir = "include/version123",
    # working_directory = "build/cmake",
    visibility = ["//visibility:public"],
    out_static_libs = ["libz.a"],
)