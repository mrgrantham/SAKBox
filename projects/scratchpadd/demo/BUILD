load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")

# generic extra padd
cc_library(
    name = "demo_padds",
    hdrs = ["include/DemoMessages.hpp", "include/ShapeLoggerPadd.hpp", "include/StoryPadd.hpp"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "include/",
    deps = [
        "//projects/data_dep_retriever",
        "//projects/scratchpadd/audio:audio_made_sounds",
        "//projects/scratchpadd/audio:audio_messages",
        "//projects/scratchpadd/basis",
    ],
)

cc_binary(
    name = "demo",
    srcs = ["src/main.cpp"],
    # The General MIDI SoundFont StoryPadd's tune plays on (MIT; see MODULE.bazel)
    data = [
        "@fluidr3_mono_soundfont//file",
        "@fluidr3_mono_soundfont_license//file",
    ],
    deps = [
        "//projects/scratchpadd/basis:basis",
        "@backward",
        "//projects/scratchpadd/debug_ui",
        "//projects/scratchpadd/audio:audio",
        "//projects/scratchpadd/midi_input",
        "//projects/scratchpadd/messages:messages",
        ":demo_padds",
    ],
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