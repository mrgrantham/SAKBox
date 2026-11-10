load("@rules_cc//cc:defs.bzl", "cc_library")

# volk.c as C++: its function pointers then go in namespace volk
# (VOLK_NAMESPACE), which only C++ has
genrule(
    name = "volk_cpp",
    srcs = ["volk.c"],
    outs = ["volk.cpp"],
    cmd = "cp $< $@",
)

# volk: Vulkan's functions, loaded when the program runs (volkInitialize).
# Its function pointers are in namespace volk, not named as Vulkan's own
# functions are: Linux test builds link each library as a shared library of
# its own, so they're seen outside volk's (VOLK_DEFAULT_VISIBILITY), and
# with Vulkan's names the Vulkan loader's calls to its own functions would
# land on them. volk.h's `using namespace volk` keeps callers unchanged.
cc_library(
    name = "volk",
    srcs = [":volk_cpp"],
    hdrs = ["volk.h"],
    defines = ["VOLK_NAMESPACE"],
    includes = ["."],
    linkopts = select({
        "@platforms//os:linux": ["-ldl"],
        "//conditions:default": [],
    }),
    local_defines = ["VOLK_DEFAULT_VISIBILITY"],
    visibility = ["//visibility:public"],
    deps = ["@vulkan_headers//:vulkan_headers_no_prototypes"],
)
