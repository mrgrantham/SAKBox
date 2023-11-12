load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//tools/clang_format:clang_format.bzl", "clang_format")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")

filegroup(
    name = "shaders",
    data = glob(["include/ScratchPadd/Graphics/GL/Shaders/*.shader"]),
)

cc_library(
    name = "scratchpadd_graphics_padd",
    hdrs = glob(["include/Graphics/**/*.hpp"]),
    srcs = ["src/Graphics.cpp"],
    linkopts = select({
    "@platforms//os:windows": [
        "-DEFAULTLIB:user32.lib",
        "-DEFAULTLIB:gdi32.lib",
        "-DEFAULTLIB:shell32.lib",
    ],
    "@platforms//os:macos": [
        "-framework OpenGL",
        "-framework Cocoa",
        "-framework IOKit",
        "-framework CoreFoundation",
        "-framework CoreVideo",
    ],
    "//conditions:default": [],
}),
    strip_include_prefix = "include/",

    defines = [
        "SCRATCHPADD_GL",
        "GL_SILENCE_DEPRECATION",
    ],
    deps = [
        "@glew",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_backend_opengl3",
        ":scratchpadd",
    ],
    data = [":shaders"],
)

cc_library(
    name = "scratchpadd",
    hdrs = glob(["include/**/*.hpp"],exclude = ["include/Graphics/**/*.hpp"]),
    srcs = ["src/DataDependencies.cpp",],
    defines = [
        "PROJECT_NAME=\\\"SCRATCHPADD\\\"",
    ],
    strip_include_prefix = "include/",
    deps = [
        "@boost//:lockfree",
        "@boost//:program_options",
        "@boost//:filesystem",
        "@fmt",
        "@spdlog",
        "@bazel_tools//tools/cpp/runfiles",
    ],
)

cc_binary(
    name = "demo",
    srcs = ["src/main.cpp"],
    deps = [
        ":scratchpadd",
        "@backward",
        ":scratchpadd_graphics_padd",
    ],
)

cc_test(
  name = "Test_Base",
  size = "small",
  srcs = ["tests/Test_Base.cpp"],
  deps = ["@gtest//:gtest_main",
  ":scratchpadd"],
)

cc_test(
  name = "Test_Controls",
  size = "small",
  srcs = ["tests/Test_Controls.cpp"],
  deps = ["@gtest//:gtest_main",
  ":scratchpadd"],
)

cc_test(
  name = "Test_Messages",
  size = "small",
  srcs = ["tests/Test_Messages.cpp"],
  deps = ["@gtest//:gtest_main",
  ":scratchpadd"],
)

cc_test(
  name = "Test_Threading",
  size = "small",
  srcs = ["tests/Test_Threading.cpp"],
  deps = ["@gtest//:gtest_main",
  ":scratchpadd"],
)

# macos_application(
#     name = "macos_demo",
#     bundle_id = "com.scratchpadd.macos_demo",
#     bundle_name = "ScratchPadd",
#     minimum_os_version = "13.3.1",
#     sources = ["src/main.cpp"],
#     resources = [":shaders"],
#     deps = [
#         "//MyFramework",
#         "@swift//:stdlib",
#     ],
# )

# load(
#     "//apple:versioning.bzl",
#     "apple_bundle_version",
# )

# licenses(["notice"])

# objc_library(
#     name = "Sources",
#     srcs = [
#         "Sources/AppDelegate.h",
#         "Sources/AppDelegate.m",
#         "Sources/main.m",
#     ],
# )

# apple_bundle_version(
#     name = "HelloWorldVersion",
#     build_version = "1.0",
# )

# macos_application(
#     name = "HelloWorld",
#     app_icons = ["//examples/resources:MacAppIcon.xcassets"],
#     bundle_id = "com.example.hello-world",
#     infoplists = [":Info.plist"],
#     minimum_os_version = "10.11",
#     version = ":HelloWorldVersion",
#     deps = [":Sources"],
# )

clang_format(name = "flexmath")
