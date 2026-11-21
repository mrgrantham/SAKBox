"""ThinLTO (clang's link-time optimisation) for optimised builds on the Mac.

Splitting Cosmic Hex's rules into files by part of the turn moved small
helpers the compiler used to inline into other files, and the simulator
ran about 4% slower. ThinLTO inlines across files when linking: measured,
the split simulator ran about 3% faster than before the split, every
game's log identical. Only for -c opt on macOS (clang): the Linux checks
build with GCC, which can't read clang's LTO objects, and debug builds
needn't pay the slower link.

    load("//tools/lto:lto.bzl", "LTO_COPTS", "LTO_LINKOPTS")
    cc_library(..., copts = LTO_COPTS)       # the hot code
    cc_binary(..., linkopts = LTO_LINKOPTS)  # where it's linked
"""

LTO_COPTS = select({
    "//tools/lto:opt_on_macos": ["-flto=thin"],
    "//conditions:default": [],
})

LTO_LINKOPTS = select({
    "//tools/lto:opt_on_macos": ["-flto=thin"],
    "//conditions:default": [],
})
