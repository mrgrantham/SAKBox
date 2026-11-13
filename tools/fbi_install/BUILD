load("@rules_python//python:py_library.bzl", "py_library")
load("@rules_python//python:py_test.bzl", "py_test")

# Builds 3DS apps and installs them over Wi-Fi with FBI (see README.md); run
# it with python3, as it runs Bazel itself
py_library(
    name = "fbi_install",
    srcs = ["fbi_install.py"],
    imports = ["."],  # imported as `fbi_install`
    deps = ["//tools/local_network"],
)

py_test(
    name = "fbi_install_test",
    size = "small",
    srcs = ["fbi_install_test.py"],
    deps = [":fbi_install"],
)
