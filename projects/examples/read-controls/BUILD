load("//tools/devkitpro:defs.bzl", "devkitpro_3ds_app")

# Nintendo 3DS homebrew (libctru's read-controls sample): shows which buttons
# are pressed and where the circle pad is. Builds read-controls.3dsx with the
# devkitPro toolchain pinned in //tools/devkitpro, which needs Docker running.
devkitpro_3ds_app(
    name = "read-controls",
    srcs = ["Makefile"] + glob(["source/**"]),
)
