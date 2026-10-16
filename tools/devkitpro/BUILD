load("@rules_python//python:py_binary.bzl", "py_binary")

# Runs devkitPro's make inside the pinned image (see defs.bzl). Python, so
# it works the same on macOS, Linux and Windows.
py_binary(
    name = "build_3ds",
    srcs = ["build_3ds.py"],
    visibility = ["//visibility:public"],
)
