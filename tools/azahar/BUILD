# Azahar, the 3DS emulator `bazel run` opens 3DS apps in (see README.md):
# downloaded by Bazel for the machine it runs on, pinned in MODULE.bazel

config_setting(
    name = "macos_arm64",
    constraint_values = ["@platforms//os:macos", "@platforms//cpu:arm64"],
)

config_setting(
    name = "macos_x86_64",
    constraint_values = ["@platforms//os:macos", "@platforms//cpu:x86_64"],
)

config_setting(
    name = "linux_x86_64",
    constraint_values = ["@platforms//os:linux", "@platforms//cpu:x86_64"],
)

# Nothing, where Azahar publishes no build Bazel can use (Linux on ARM,
# Windows): the launcher then says so
filegroup(name = "none")

alias(
    name = "azahar",
    actual = select({
        ":macos_arm64": "@azahar_macos_arm64//:azahar",
        ":macos_x86_64": "@azahar_macos_x86_64//:azahar",
        ":linux_x86_64": "@azahar_linux_x86_64//file",
        "//conditions:default": ":none",
    }),
    visibility = ["//visibility:public"],
)
