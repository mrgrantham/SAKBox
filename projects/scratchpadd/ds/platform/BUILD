# What ScratchPadd needs on a Nintendo DS that devkitPro doesn't provide, as
# sources for DS apps' Makefiles (see //tools/devkitpro): std::atomic's
# library functions (src/atomics.c), and a clock for std::chrono (src/clock.c).
filegroup(
    name = "sources",
    srcs = glob(["src/**"]),
    visibility = ["//visibility:public"],
)
