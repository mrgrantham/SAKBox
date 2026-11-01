load("@rules_cc//cc:defs.bzl", "cc_library")

# cgltf (https://github.com/jkuhlmann/cgltf): reads glTF 2.0, a single
# header, included as "cgltf.h". Define CGLTF_IMPLEMENTATION in one source
# file that includes it.
cc_library(
    name = "cgltf",
    hdrs = ["cgltf.h"],
    visibility = ["//visibility:public"],
)
