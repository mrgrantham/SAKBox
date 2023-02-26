load("@rules_cc//cc:defs.bzl", "cc_library", "objc_library")
load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

# objc_library(
#     name = "glfw_cocoa",
#     hdrs = [
#         "include/GLFW/glfw3.h",
#         "include/GLFW/glfw3native.h",
#         "src/cocoa_joystick.h",
#         "src/nsgl_context.h",
#         "src/cocoa_platform.h",
#         "src/internal.h",
#         "src/posix_thread.h",
#         "src/egl_context.h",
#         "src/osmesa_context.h",
#         "src/mappings.h",
#     ],
#     srcs = [
#         "src/cocoa_joystick.m",
#         "src/nsgl_context.m",
#         "src/cocoa_monitor.m",
#         "src/cocoa_window.m",
#         "src/cocoa_init.m",
#         "src/posix_thread.c",
#         "src/cocoa_time.c",
#         "src/egl_context.c",
#         "src/osmesa_context.c",
#         "src/context.c",
#         "src/init.c",
#         "src/input.c",
#         "src/vulkan.c",
#         "src/window.c",
#         "src/monitor.c",
#     ],
#     defines = [
#         "_GLFW_COCOA",
#     ],
#     copts = [
#         "-fno-objc-arc",
#     ],
#     visibility = [
#         "//visibility:private",
#     ],
# )

# cc_library(
#     name = "glfw_src",
#     srcs = [
#         "src/context.c",
#         "src/egl_context.c",
#         "src/init.c",
#         "src/input.c",
#         "src/osmesa_context.c",
#         "src/monitor.c",
#         "src/vulkan.c",
#         "src/window.c",
#         "src/xkb_unicode.c",
#     ] + select({
#         "@platforms//os:windows": [
#             "src/win32_init.c",
#             "src/win32_joystick.c",
#             "src/win32_monitor.c",
#             "src/win32_thread.c",
#             "src/win32_time.c",
#             "src/win32_window.c",
#             "src/wgl_context.c",
#         ],
#         "@platforms//os:macos": [
#             "src/cocoa_time.c",
#             "src/posix_thread.c",
#         ],
#         "//conditions:default": [
#             "src/glx_context.c",
#             "src/linux_joystick.c",
#             "src/posix_thread.c",
#             "src/posix_time.c",
#             "src/x11_init.c",
#             "src/x11_monitor.c",
#             "src/x11_window.c",
#         ],
#     }),
#     hdrs = [
#         "include/GLFW/glfw3.h",
#         "include/GLFW/glfw3native.h",
#         "src/egl_context.h",
#         "src/internal.h",
#         "src/osmesa_context.h",
#         "src/mappings.h",
#         "src/xkb_unicode.h"
#     ] + select({
#         "@platforms//os:windows": [
#             "src/win32_joystick.h",
#             "src/win32_platform.h",
#             "src/wgl_context.h", 
#         ],
#         "@platforms//os:macos": [
#             "src/cocoa_joystick.h",
#             "src/cocoa_platform.h",
#             "src/glx_context.h",
#             "src/nsgl_context.h",
#             "src/null_joystick.h",
#             "src/null_platform.h",
#             "src/posix_thread.h",
#             "src/wl_platform.h",
#         ],
#         "//conditions:default": [
#             "src/glx_context.h",
#             "src/linux_joystick.h",
#             "src/posix_thread.h",
#             "src/posix_time.h",
#             "src/x11_platform.h",
#         ],
#     }),
#     defines = select({
#         "@platforms//os:windows": [
#             "_GLFW_WIN32",
#         ],
#         "@platforms//os:macos": [
#             "_GLFW_COCOA",
#             "_GLFW_NSGL",
#             "_GLFW_NO_DLOAD_WINMM",
#             "_GLFW_USE_OPENGL",
#         ],
#         "//conditions:default": [
#             "_GLFW_HAS_XF86VM",
#             "_GLFW_X11",
#         ],
#     }),
#     linkopts = select({
#         "@platforms//os:windows": [
#             "-DEFAULTLIB:user32.lib",
#             "-DEFAULTLIB:gdi32.lib",
#             "-DEFAULTLIB:shell32.lib",
#         ],
#         "@platforms//os:macos": [
#             "-framework OpenGL",
#             "-framework Cocoa",
#             "-framework IOKit",
#             "-framework CoreFoundation",
#             "-framework CoreVideo",
#         ],
#         "//conditions:default": [],
#     }),
#     deps = select({
#         "@platforms//os:macos": [
#             ":glfw_cocoa",
#         ],
#         "//conditions:default": [],
#     }),
#     visibility = ["//visibility:private"],
# )

# cc_library(
#     name = "glfw",
#     hdrs = [
#         "include/GLFW/glfw3.h",
#         "include/GLFW/glfw3native.h",
#     ],
#     deps = [
#         ":glfw_src",
#     ],
#     strip_include_prefix = "include",
#     visibility = ["//visibility:public"],
# )

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Shows a standard library
cmake(
    name = "glfw",
    lib_source = ":sources",
    # pass include/version123 to C/C++ provider as include directory
    # out_include_dir = "include/version123",
    # working_directory = "build/cmake",
    visibility = ["//visibility:public"],
    out_static_libs = ["libglfw3.a"],
    defines = select({
        "@platforms//os:windows": [
            "_GLFW_WIN32",
        ],
        "@platforms//os:macos": [
            "_GLFW_COCOA",
            "_GLFW_NSGL",
            "_GLFW_NO_DLOAD_WINMM",
            "_GLFW_USE_OPENGL",
        ],
        "//conditions:default": [
            "_GLFW_HAS_XF86VM",
            "_GLFW_X11",
        ],
    }),
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