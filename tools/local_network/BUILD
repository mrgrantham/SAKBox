load("@rules_python//python:py_library.bzl", "py_library")
load("@rules_python//python:py_test.bzl", "py_test")

# Connections to a device on the local network, through nc where macOS keeps
# Python off it (local_network.py): for fbi_install and console_log
py_library(
    name = "local_network",
    srcs = ["local_network.py"],
    imports = ["."],  # imported as `local_network`
    visibility = ["//tools:__subpackages__"],
)

py_test(
    name = "local_network_test",
    size = "small",
    srcs = ["local_network_test.py"],
    deps = [":local_network"],
)
