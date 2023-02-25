load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "source",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

configure_make(
    name = "binutils",
    configure_in_place = True,
    lib_source = ":source",
    configure_options = ["CC=\"gcc\"","CXX=\"gcc\"","CFLAGS='-g -O2'","AR=\"ar\"","CXXGLAGS='-g -O2'"],
    visibility = ["//visibility:public"],
    out_static_libs = ["libbfd.a"],
)