load("//tools/devkitpro:defs.bzl", "devkitpro_nds_app")

# Cyclone's ragdoll on a Nintendo DS, in padds: the physics on its own
# thread, timed, and drawn by the DS's 3D hardware. Needs Docker running (see
# //tools/devkitpro); `bazel run` opens it in melonDS.
devkitpro_nds_app(
    name = "cyclone-ds-ragdoll",
    srcs = ["Makefile"] + glob(["source/**"]),
    extra_srcs = {
        "//projects/cyclone:sources": "cyclone",
        "//projects/flexmath:fixed_point_sources": "flexmath",
        "//projects/scratchpadd/basis:sources": "scratchpadd/basis",
        "//projects/scratchpadd/messages:sources": "scratchpadd/messages",
        "//projects/scratchpadd/utilities:sources": "scratchpadd/utilities",
        "//projects/scratchpadd/ds/platform:sources": "scratchpadd_ds/platform",
        "@spdlog//:sources": "third_party/spdlog",
    },
)
