load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")


load("@rules_foreign_cc//foreign_cc:defs.bzl","configure_make")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

configure_make(
    name = "libpng",
    configure_in_place = True,
    lib_source = ":sources",
    configure_options = ["CC=\"gcc\"","CXX=\"gcc\"","CFLAGS=","AR=\"ar\""],
    # targets = [
    #     "install",
    # ],
    # out_lib_dir = "libpng",
    # configure_options = ["--disable-static"],
    # out_shared_libs = ["libpng16.dylib","libpng16.16.dylib"],
    # cc_toolchain = "//:gcc",
    visibility = ["//visibility:public"],
    deps = ["@zlib"],
)

# cc_library(
#     name = "libpng",
#     srcs = glob([
#         "libpng/*.c",
#         "libpng/*.h",
#     ]),
#     hdrs = glob([
#         "libpng/*.h",
#     ]),
#     includes = [
#         "libpng",
#     ],
#     strip_include_prefix = "libpng",
#     visibility = ["//visibility:public"],
#     defines = [
#         "PNG_USE_GLOBAL_ARRAYS",
#     ],
#     copts = [
#         "-O2",
#     ],
#     # linkopts = [
#     #     "-Wl,-rpath,@loader_path/",
#     # ],
#     deps = [
#         # "//third_party:zlib",
#     ],
# )

