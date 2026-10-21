load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Shows a standard library
cmake(
    name = "yamlcpp",
    lib_source = ":sources",
    # pass include/version123 to C/C++ provider as include directory
    # out_include_dir = "include/version123",
    # working_directory = "build/cmake",
    visibility = ["//visibility:public"],
    out_static_libs = ["libyaml-cpp.a"],
    # Only the library is used. Its command-line tools (util/) also failed to
    # link on Linux with GCC, as C without the C++ standard library.
    cache_entries = {"YAML_CPP_BUILD_TESTS": "OFF", "YAML_CPP_BUILD_TOOLS": "OFF"},
)