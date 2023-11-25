load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")

# generic extra padd
cc_library(
    name = "story",
    hdrs = glob(["include/StoryPadd.hpp"]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "include/",
    deps = [
        "//projects/scratchpadd/basis",
    ],
)

cc_binary(
    name = "demo",
    srcs = ["src/main.cpp"],
    deps = [
        "//projects/scratchpadd/basis:basis",
        "@backward",
        "//projects/scratchpadd/graphics:graphics",
        "//projects/scratchpadd/audio:audio",
        "//projects/scratchpadd/messages:messages",
        ":story",
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