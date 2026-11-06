load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")
load("@//tools/windows:defs.bzl", "NOT_ON_WINDOWS")

filegroup(
    name = "source",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

configure_make(
    name = "binutils",
    # rules_foreign_cc's builds hang on Windows on ARM (its Ninja wrapper
    # never returns), so this, and what uses it, isn't built on Windows
    target_compatible_with = NOT_ON_WINDOWS,
    configure_in_place = True,
    lib_source = ":source",
    configure_options = ["CC=\"gcc\"","CXX=\"gcc\"","CFLAGS='-g -O2'","AR=\"ar\"","CXXGLAGS='-g -O2'"],
    visibility = ["//visibility:public"],
    out_static_libs = ["libbfd.a"],
)