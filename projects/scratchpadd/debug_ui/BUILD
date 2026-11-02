load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("//tools/embed_file:embed_file.bzl", "embed_file")

# Dear ImGui on a desktop scene Display, with ScratchPadd's look (Theme.hpp):
# control panels for each padd's controls, and DisplayPadd, a window for any
# system. Desktop only: never compiled for the DS. See the README's Debug UI
# section.
cc_library(
    name = "debug_ui",
    hdrs = glob(["include/**/*.hpp"]),
    srcs = [
        "src/ControlPanel.cpp",
        "src/DebugUi.cpp",
        "src/ImGuiBackend.hpp",
        "src/Theme.cpp",
    ] + select({
        "//projects/scratchpadd/scene/desktop:metal": ["src/ImGuiMetal.cpp"],
        "//conditions:default": ["src/ImGuiOpenGl.cpp"],
    }),
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":jost_medium",
        ":questrial",
        "//projects/scratchpadd/basis",
        "//projects/scratchpadd/messages",
        "//projects/scratchpadd/scene",
        "//projects/scratchpadd/scene/desktop",
        "@imgui",
        "@imgui//:imgui_backend_glfw",
        "@imgui//:imgui_stdlib",
    ] + select({
        "//projects/scratchpadd/scene/desktop:metal": [
            "//projects/scratchpadd/scene/metal",
            "@imgui//:imgui_backend_metal",
            "@metal-cpp",
        ],
        "//conditions:default": ["@imgui//:imgui_backend_opengl3"],
    }),
)

# The theme's fonts, compiled in (see Theme.hpp). Both are under the SIL Open
# Font License, which goes with them (into the runfiles of every program that
# uses the debug UI).
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
        ":debug_ui",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "ControlPanels_test",
    size = "small",
    srcs = ["tests/ControlPanels_test.cpp"],
    deps = [
        ":debug_ui",
        "@gtest//:gtest_main",
    ],
)
