
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
  name = "com_google_googletest",
  urls = ["https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip"],
  strip_prefix = "googletest-1.13.0",
)

http_archive(
    name = "cyclone",
    url = "https://github.com/idmillington/cyclone-physics/archive/refs/heads/master.zip",
    sha256 = "faeda03d28499b7120208d7e70759bf1832511b6d64f546ab32103e9ba400f03",
    strip_prefix = "cyclone-physics-master",
    build_file = "@//third_party:cyclone.BUILD",
)