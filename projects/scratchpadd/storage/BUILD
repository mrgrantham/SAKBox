load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

# An app's storage (see README.md): where its files and logs go (Folders),
# whole files saved and loaded (Files), files that grow written in batches
# (AppendFile), and the sources StoragePadd writes for (Source, sources()).
# Plain C++, no padds; tested on desktops.
cc_library(
    name = "storage",
    srcs = [
        "src/AppendFile.cpp",
        "src/Files.cpp",
        "src/Folders.cpp",
        "src/Sources.cpp",
    ],
    hdrs = [
        "include/ScratchPadd/Storage/AppendFile.hpp",
        "include/ScratchPadd/Storage/Files.hpp",
        "include/ScratchPadd/Storage/Folders.hpp",
        "include/ScratchPadd/Storage/Source.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = ["//projects/scratchpadd/utilities"],
)

# The padd that does the app's writing when nothing else has work: its
# sources' (the log's file) and whole files saved for any padd
cc_library(
    name = "storage_padd",
    hdrs = [
        "include/ScratchPadd/Storage/Messages.hpp",
        "include/ScratchPadd/Storage/StoragePadd.hpp",
    ],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
    deps = [
        ":storage",
        "//projects/scratchpadd/basis",
    ],
)

cc_test(
    name = "StoragePadd_test",
    size = "small",
    srcs = ["tests/StoragePadd_test.cpp"],
    deps = [
        ":storage_padd",
        "//projects/scratchpadd/basis",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Folders_test",
    size = "small",
    srcs = ["tests/Folders_test.cpp"],
    deps = [
        ":storage",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "Files_test",
    size = "small",
    srcs = ["tests/Files_test.cpp"],
    deps = [
        ":storage",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "AppendFile_test",
    size = "small",
    srcs = ["tests/AppendFile_test.cpp"],
    deps = [
        ":storage",
        "@gtest//:gtest_main",
    ],
)

# For the 3DS's and DS's Makefile builds (devkitpro_*_app's extra_srcs, with
# FOLDER/src in the Makefile's SOURCES)
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = ["//visibility:public"],
)
