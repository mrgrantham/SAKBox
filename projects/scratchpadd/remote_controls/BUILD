load("@rules_cc//cc:defs.bzl", "cc_binary")

# A console's controls on the Mac (see README.md): connects to an app's
# RemoteControlsPadd (a 3DS's, or Azahar's on this Mac) and shows them in
# the Controls window.
#   bazel run //projects/scratchpadd/remote_controls -- 192.168.4.25
cc_binary(
    name = "remote_controls",
    srcs = ["src/main.cpp"],
    deps = [
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/debug_log",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/messages",
        "//projects/scratchpadd/network",
        "//projects/scratchpadd/network:network_padd",
    ],
)
