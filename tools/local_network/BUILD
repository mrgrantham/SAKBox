load("@rules_python//python:py_library.bzl", "py_library")

# Connections to a device on the local network, through nc where macOS keeps
# Python off it (local_network.py): for fbi_install and console_log
py_library(
    name = "local_network",
    srcs = ["local_network.py"],
    imports = ["."],  # imported as `local_network`
    visibility = ["//tools:__subpackages__"],
)
