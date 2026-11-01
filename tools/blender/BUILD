# Blender, for exporting models in the build (see README.md and
# //tools/assets): run_blender.sh runs Bazel's Blender for this machine
# (@blender, downloaded and pinned by blender.bzl), or BLENDER.

exports_files(["run_blender.sh"])

alias(
    name = "program",
    actual = "@blender//:program",
    visibility = ["//visibility:public"],
)
