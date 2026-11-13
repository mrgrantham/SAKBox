load("@rules_python//python:py_library.bzl", "py_library")
load("@rules_python//python:py_test.bzl", "py_test")

# Reads a 3DS app's debug log over Wi-Fi (see README.md); run it with
# python3 from the repo
py_library(
    name = "console_log",
    srcs = ["console_log.py"],
    imports = ["."],  # imported as `console_log`
    deps = ["//tools/local_network"],
)

py_test(
    name = "console_log_test",
    size = "small",
    srcs = ["console_log_test.py"],
    deps = [":console_log"],
)
