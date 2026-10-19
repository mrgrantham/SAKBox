
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "WClasses",
    srcs = glob(["src/WClasses/*.cpp",
    ]),
    hdrs = glob([
        "src/WClasses/*.h",
    ]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "src",
)

cc_library(
    name = "gui",
    srcs = glob(["src/gui/*.cpp",
    ]),
    hdrs = glob([
        "src/gui/*.h",
    ]),
    deps = ["@sdl2","//projects/waffles:WClasses","@fmt"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "src",
)

cc_library(
    name = "png",
    srcs = glob(["src/png/*.cpp",
    ]),
    hdrs = glob([
        "src/png/*.h",
    ]),
    deps = ["@libpng","//projects/waffles:WClasses",
],
    visibility = ["//visibility:public"],
    strip_include_prefix = "src",
)

cc_binary(
    name = "audio",
    srcs = ["src/audio/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "cluster",
    srcs = ["src/cluster/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "dimred",
    srcs = ["src/dimred/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "learn",
    srcs = ["src/learn/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "plot",
    srcs = ["src/plot/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "recommend",
    srcs = ["src/recommend/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "sparse",
    srcs = ["src/sparse/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "transform",
    srcs = ["src/transform/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_binary(
    name = "wizard",
    srcs = ["src/wizard/main.cpp"],
    deps = [
        ":WClasses",
    ],
)

cc_test(
    name = "test",
    srcs = ["src/test/main.cpp"],
    # The command-line tools its command-line tests run (see useBazelTools)
    data = [":transform", ":recommend", ":sparse", ":learn", ":dimred"],
    deps = [
        ":WClasses",
        "@fmtlog",
        "@gtest",
        "@spdlog",
    ],
)

# SAKBox's tests of Waffles (alongside upstream's suite, :test above), one
# file per area of the library, named after its header: tests/GMatrix_test.cpp
# for GMatrix.h and so on. A test's comment says why it exists, such as the
# bug it caught.
[cc_test(
    name = name,
    size = "small",
    srcs = ["tests/%s.cpp" % name],
    deps = [
        ":WClasses",
        "@gtest//:gtest_main",
    ],
) for name in [
    "G3D_test",
    "GApp_test",
    "GCluster_test",
    "GDom_test",
    "GError_test",
    "GFunction_test",
    "GHashTable_test",
    "GHolders_test",
    "GHttp_test",
    "GImage_test",
    "GLearner_test",
    "GMath_test",
    "GMatrix_test",
    "GNeuralNet_test",
    "GRand_test",
    "GRayTrace_test",
    "GRecommender_test",
    "GRegion_test",
    "GStemmer_test",
    "GTime_test",
    "GTransform_test",
    "GVec_test",
    "sha2_test",
]]
