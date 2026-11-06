load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@//tools/windows:defs.bzl", "NOT_ON_WINDOWS")


load("@rules_foreign_cc//foreign_cc:defs.bzl","configure_make")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

configure_make(
    name = "libpng",
    # rules_foreign_cc's builds hang on Windows on ARM (its Ninja wrapper
    # never returns), so this, and what uses it, isn't built on Windows
    target_compatible_with = NOT_ON_WINDOWS,
    configure_in_place = True,
    lib_source = ":sources",
    # Use plain ar: autotools cannot drive Apple's libtool, which Bazel uses as the archiver on macOS.
    configure_options = ["CFLAGS=", "AR=\"ar\"", "AR_FLAGS=cr"],
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

