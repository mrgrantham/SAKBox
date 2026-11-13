# NdspOutput: the audio core's output on a Nintendo 3DS, through libctru's
# ndsp. Built only by devkitPro (3DS apps stage these with extra_srcs), so
# it's sources here rather than a cc_library.
filegroup(
    name = "ndsp_output_sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = [
        "//projects/cyclone/3ds:__pkg__",  # the ragdoll on a 3DS
        "//projects/scratchpadd:__subpackages__",
    ],
)
