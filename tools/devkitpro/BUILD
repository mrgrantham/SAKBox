load("@rules_python//python:py_binary.bzl", "py_binary")
load("@rules_python//python:py_library.bzl", "py_library")
load(":defs.bzl", "devkitpro_cia_tools")

# Runs scripts inside the pinned devkitPro image (see defs.bzl). Python, so
# it works the same on macOS, Linux and Windows.
py_library(
    name = "devkitpro_image",
    srcs = ["devkitpro_image.py"],
    imports = ["."],  # imported as `devkitpro_image`
)

# devkitPro's make, and the CIA, for a 3DS app
py_binary(
    name = "build_3ds",
    srcs = ["build_3ds.py"],
    visibility = ["//visibility:public"],
    deps = [":devkitpro_image"],
)

# devkitPro's make, for a DS app
py_binary(
    name = "build_nds",
    srcs = ["build_nds.py"],
    visibility = ["//visibility:public"],
    deps = [":devkitpro_image"],
)

py_binary(
    name = "build_cia_tools",
    srcs = ["build_cia_tools.py"],
    deps = [":devkitpro_image"],
)

# makerom and bannertool, built from source in the image (once; Bazel
# caches them), for making CIAs
devkitpro_cia_tools(
    name = "cia_tools",
    tags = ["devkitpro"],
    visibility = ["//visibility:public"],
)

# What `bazel run` does for a 3DS app (opens it in the Azahar emulator) and
# for a DS app (in melonDS)
exports_files([
    "run_in_azahar.sh",
    "run_in_melonds.sh",
])
