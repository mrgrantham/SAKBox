# Azahar (https://azahar-emu.org), the Nintendo 3DS emulator, as published
# for macOS: Azahar.app. GPL-2.0, run as a separate program, not linked.
filegroup(
    name = "azahar",
    srcs = ["Azahar.app/Contents/MacOS/azahar"],
    data = glob(["Azahar.app/**"]),  # the rest of the app bundle
    visibility = ["//visibility:public"],
)
