load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake","make")

# filegroup(
#     name = "sources",
#     srcs = glob(["**"]),
#     visibility = ["//simple_make:__subpackages__"],
# )

cc_library(
    name = "variadic_table",
    hdrs = ["include/VariadicTable.h"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "include",
)

# # Shows a standard library
# make(
#     name = "variadic_table",
#     lib_source = ":sources",
#     # pass include/version123 to C/C++ provider as include directory
#     # out_include_dir = "include/version123",
#     # working_directory = "build/cmake",
#     visibility = ["//visibility:public"],
#     # out_static_libs = ["libGLEW.a"],
# )

# make(
#     name = "variadic_table",
#     # build_data = [
#     #     "//make_simple/code:cxx_wrapper.sh",
#     #     "README.md",
#     # ],
#     # copts = [
#     #     "-DREQUIRED_DEFINE",
#     # ],
#     # env = {
#     #     "CXX_WRAPPER": "$(execpath //make_simple/code:cxx_wrapper.sh)",
#     #     "README_DIR": "$$(dirname $(execpath README.md))",
#     # },
#     lib_source = ":sources",
#     visibility = ["//visibility:public"],
#     # out_data_dirs = ["share"],
#     # out_static_libs = ["liba.a"],
# )