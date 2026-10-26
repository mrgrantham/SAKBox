# melonDS (https://melonds.kuribo64.net), the Nintendo DS emulator, as
# published for macOS: melonDS.app, for Apple Silicon and Intel. GPL-3.0, run
# as a separate program, not linked.
filegroup(
    name = "melonds",
    srcs = ["melonDS.app/Contents/MacOS/melonDS"],
    data = glob(["melonDS.app/**"]),  # the rest of the app bundle
    visibility = ["//visibility:public"],
)
