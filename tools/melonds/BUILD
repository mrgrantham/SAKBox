# melonDS, the DS emulator `bazel run` opens DS apps in (see README.md):
# downloaded by Bazel for the machine it runs on, pinned in MODULE.bazel

config_setting(
    name = "macos",
    constraint_values = ["@platforms//os:macos"],
)

config_setting(
    name = "linux_x86_64",
    constraint_values = ["@platforms//os:linux", "@platforms//cpu:x86_64"],
)

# Nothing, where Bazel has no melonDS for the machine: the launcher then
# says so
filegroup(name = "none")

alias(
    name = "melonds",
    actual = select({
        ":macos": "@melonds_macos//:melonds",  # one app for Apple Silicon and Intel
        ":linux_x86_64": "@melonds_linux_x86_64//:melonds",
        "//conditions:default": ":none",
    }),
    visibility = ["//visibility:public"],
)
