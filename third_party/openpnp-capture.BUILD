load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Shows a standard library
cmake(
    name = "openpnp-capture",
    lib_source = ":sources",
    # pass include/version123 to C/C++ provider as include directory
    # out_include_dir = "include/version123",
    # working_directory = "build/cmake",
    # out_lib_dir = "sdl2/lib",
    # install = False,
    visibility = ["//visibility:public"],
    # out_shared_libs = select({"@platforms//os:macos":["libopenpnp-capture.dylib"],"//conditions:default": ["libopenpnp-capture.so"]}),
    # out_static_libs = ["libSDL3.a"],
    # cache_entries = {"SDL_SHARED":"ON","SDL_STATIC":"ON","SDL_METAL":"ON"},
    targets = [
        "",
    ],
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
            "-Wl,-framework,QuartzCore",
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