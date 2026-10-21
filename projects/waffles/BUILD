
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
    deps = ["@libstemmer"], # WStemmer
)

cc_library(
    name = "Gui",
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
    name = "WImagePng",
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

# waffles_generate: makes datasets (src/depends/ because it needs libpng, through :WImagePng)
cc_binary(
    name = "generate",
    srcs = ["src/depends/generate/main.cpp"],
    deps = [
        ":WClasses",
        ":WImagePng",
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

# The PNG library's test needs :WImagePng as well
cc_test(
    name = "WImagePng_test",
    size = "small",
    srcs = ["tests/WImagePng_test.cpp"],
    deps = [
        ":WImagePng",
        ":WClasses",
        "@gtest//:gtest_main",
    ],
)

# SAKBox's tests of Waffles (alongside upstream's suite, :test above), one
# file per area of the library, named after its header: tests/WMatrix_test.cpp
# for WMatrix.h and so on. A test's comment says why it exists, such as the
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
    "W3D_test",
    "WApp_test",
    "WCluster_test",
    "WDom_test",
    "WError_test",
    "WFunction_test",
    "WHashTable_test",
    "WHolders_test",
    "WHttp_test",
    "WImage_test",
    "WLearner_test",
    "WMath_test",
    "WMatrix_test",
    "WNeuralNet_test",
    "WRand_test",
    "WRayTrace_test",
    "WRecommender_test",
    "WRegion_test",
    "WStemmer_test",
    "WTime_test",
    "WTransform_test",
    "WVec_test",
    "sha2_test",
]]
