# Cyclone's ragdoll on a DS or 3DS, in padds: one app's source, which the
# DS's and the 3DS's builds (../ds, ../3ds) each compile in their Makefile
# builds (as `ragdoll`, in their SOURCES)
filegroup(
    name = "sources",
    srcs = glob(["source/**"]),
    visibility = [
        "//projects/cyclone/3ds:__pkg__",
        "//projects/cyclone/ds:__pkg__",
    ],
)
