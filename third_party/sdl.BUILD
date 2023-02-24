# load("@rules_cc//cc:defs.bzl", "cc_library")

# cc_library(
#     name = "sdl2",
#     hdrs = glob(["Headers/*.h"]),
#     includes = ["Header"], # Optional. 
#     visibility = ["//visibility:public"],
# )

load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Shows a standard library
cmake(
    name = "sdl2",
    lib_source = ":sources",
    # pass include/version123 to C/C++ provider as include directory
    # out_include_dir = "include/version123",
    # working_directory = "build/cmake",
    # out_lib_dir = "sdl2/lib",
    visibility = ["//visibility:public"],
    out_static_libs = ["libSDL2.a"],
    cache_entries = {"SDL_SHARED":"OFF","SDL_STATIC":"ON"},
    linkopts = select({
        "@platforms//os:windows": [
            "-DEFAULTLIB:user32.lib",
            "-DEFAULTLIB:gdi32.lib",
            "-DEFAULTLIB:shell32.lib",
        ],
        "@platforms//os:macos": [
            "-Wl,-framework,OpenGL",
            "-Wl,-framework,Cocoa",
            "-Wl,-framework,IOKit",
            "-Wl,-framework,CoreFoundation",
            "-Wl,-framework,CoreVideo",
            "-Wl,-framework,CoreAudio",
            "-Wl,-framework,GameController",
            "-Wl,-framework,ForceFeedback",
            "-Wl,-framework,AudioToolbox",
            "-Wl,-weak_framework,CoreHaptics",
            "-Wl,-weak_framework,Metal",
            "-Wl,-framework,Carbon",
            "-Wl,-liconv",
        ],
        "//conditions:default": [],
    }),
)