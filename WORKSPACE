workspace(name = "graphFun")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "fmt",
    branch = "master",
    patch_cmds = [
        "mv support/bazel/.bazelrc .bazelrc",
        "mv support/bazel/.bazelversion .bazelversion",
        "mv support/bazel/BUILD.bazel BUILD.bazel",
        "mv support/bazel/WORKSPACE.bazel WORKSPACE.bazel",
    ],
    # Windows related patch commands are only needed in the case MSYS2 is not installed
    patch_cmds_win = [
        "Move-Item -Path support/bazel/.bazelrc -Destination .bazelrc",
        "Move-Item -Path support/bazel/.bazelversion -Destination .bazelversion",
        "Move-Item -Path support/bazel/BUILD.bazel -Destination BUILD.bazel",
        "Move-Item -Path support/bazel/WORKSPACE.bazel -Destination WORKSPACE.bazel",
    ],
    remote = "https://github.com/fmtlib/fmt",
)

http_archive(
    name = "bazel_skylib",
    sha256 = "b8a1527901774180afc798aeb28c4634bdccf19c4d98e7bdd1ce79d1fe9aaad7",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.4.1/bazel-skylib-1.4.1.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.4.1/bazel-skylib-1.4.1.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

http_archive(
    name = "gtest",
    strip_prefix = "googletest-1.13.0",
    urls = ["https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip"],
)

http_archive(
    name = "catch2",
    strip_prefix = "Catch2-3.3.1",
    urls = ["https://github.com/catchorg/Catch2/archive/refs/tags/v3.3.1.zip"],
)

http_archive(
    name = "cyclone",
    build_file = "@//third_party:cyclone.BUILD",
    sha256 = "faeda03d28499b7120208d7e70759bf1832511b6d64f546ab32103e9ba400f03",
    strip_prefix = "cyclone-physics-master",
    url = "https://github.com/idmillington/cyclone-physics/archive/refs/heads/master.zip",
)

http_archive(
    name = "spdlog",
    build_file = "@//third_party:spdlog.BUILD",
    sha256 = "33f83c6b86ec0fbbd0eb0f4e980da6767494dc0ad063900bcfae8bc3e9c75f21",
    strip_prefix = "spdlog-1.11.0",
    url = "https://github.com/gabime/spdlog/archive/refs/tags/v1.11.0.zip",
)

http_archive(
    name = "fmtlog",
    build_file = "@//third_party:fmtlog.BUILD",
    sha256 = "c70458b9f67682093d88fd64f20fe1d387d2e7194f5a6d9f7f8a7b78a9c9d966",
    strip_prefix = "fmtlog-2.2.1",
    url = "https://github.com/MengRao/fmtlog/archive/refs/tags/v2.2.1.zip",
)