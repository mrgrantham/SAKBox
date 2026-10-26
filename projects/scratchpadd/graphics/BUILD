load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//tools/embed_file:embed_file.bzl", "embed_file")


filegroup(
    name = "shaders",
    data = glob(["include/ScratchPadd/Graphics/GL/Shaders/**"]),
)

cc_library(
    name = "graphics",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = [
        "src/Graphics.cpp",
        "src/Theme.cpp",
    ],
    visibility = ["//visibility:public"],
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
        "@imgui//:imgui_stdlib",
        "//projects/scratchpadd/basis:basis",
        "//projects/scratchpadd/messages:messages",
        "//projects/data_dep_retriever:data_dep_retriever",
        ":jost_medium",
        ":questrial",
    ],
    data = [":shaders"],
)
# The theme's fonts, compiled in (see Theme.hpp). Both are under the SIL Open
# Font License, which goes with them (into the runfiles of every program that
# uses the graphics library).
embed_file(
    name = "jost_medium",
    src = "@jost_medium_font//file",
    data = ["@jost_font_license//file"],
    namespace = "Theme::Fonts",
    symbol = "jostMedium",
)

embed_file(
    name = "questrial",
    src = "@questrial_font//file",
    data = ["@questrial_font_license//file"],
    namespace = "Theme::Fonts",
    symbol = "questrial",
)

cc_test(
    name = "Theme_test",
    size = "small",
    srcs = ["tests/Theme_test.cpp"],
    deps = [
        ":graphics",
        "@gtest//:gtest_main",
    ],
)
