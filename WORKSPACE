workspace(name = "monograntham")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

# rules_python_version = "740825b7f74930c62f44af95c9a4c1bd428d2c53" # Latest @ 2021-06-23
# http_archive(
#     name = "rules_python",
#     sha256 = "09a3c4791c61b62c2cbc5b2cbea4ccc32487b38c7a2cc8f87a794d7a659cc742",
#     strip_prefix = "rules_python-{}".format(rules_python_version),
#     url = "https://github.com/bazelbuild/rules_python/archive/{}.zip".format(rules_python_version),
# )

# load("@rules_python//python:repositories.bzl", "python_register_toolchains")


http_archive(
    name = "rules_foreign_cc",
    sha256 = "476303bd0f1b04cc311fc258f1708a5f6ef82d3091e53fd1977fa20383425a6a",
    strip_prefix = "rules_foreign_cc-0.10.1",
    url = "https://github.com/bazelbuild/rules_foreign_cc/releases/download/0.10.1/rules_foreign_cc-0.10.1.tar.gz",
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

http_archive(
    name = "build_bazel_rules_apple",
    sha256 = "9e26307516c4d5f2ad4aee90ac01eb8cd31f9b8d6ea93619fc64b3cbc81b0944",
    url = "https://github.com/bazelbuild/rules_apple/releases/download/2.2.0/rules_apple.2.2.0.tar.gz",
)

load(
    "@build_bazel_rules_apple//apple:repositories.bzl",
    "apple_rules_dependencies",
)

apple_rules_dependencies()

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)

apple_support_dependencies()

git_repository(
    name = "fmt",
    branch = "master",
    patch_cmds = [
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
    name = "binutils",
    strip_prefix = "binutils-2.40",
    build_file = "@//third_party:binutils.BUILD",
    urls = ["https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.xz"],
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

http_archive(
    name = "imgui",
    build_file = "@//third_party:imgui.BUILD",
    strip_prefix = "imgui-1.90",
    urls = [
        "https://github.com/ocornut/imgui/archive/refs/tags/v1.90.zip",
    ],
)

new_git_repository(
    name = "glfw",
    build_file = "@//third_party:glfw.BUILD",
    remote = "https://github.com/glfw/glfw.git",
    tag = "3.3.8",
)

http_archive(
    name = "com_github_nelhage_rules_boost",
    strip_prefix = "rules_boost-96e9b631f104b43a53c21c87b01ac538ad6f3b48",
    sha256 = "5ea00abc70cdf396a23fb53201db19ebce2837d28887a08544429d27783309ed",
    # Replace the commit hash in both places (below) with the latest, rather than using the stale one here.
    # Even better, set up Renovate and let it do the work for you (see "Suggestion: Updates" in the README).
    url = "https://github.com/nelhage/rules_boost/archive/96e9b631f104b43a53c21c87b01ac538ad6f3b48.tar.gz",
    # When you first run this tool, it'll recommend a sha256 hash to put here with a message like: "DEBUG: Rule 'com_github_nelhage_rules_boost' indicated that a canonical reproducible form can be obtained by modifying arguments sha256 = ..."
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()

http_archive(
    name = "glew",
    build_file = "@//third_party:glew.BUILD",
    sha256 = "a9046a913774395a095edcc0b0ac2d81c3aacca61787b39839b941e9be14e0d4",
    strip_prefix = "glew-2.2.0",
    urls = ["https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip"],
)

new_git_repository(
    name = "backward",
    build_file = "@//third_party:backward-cpp.BUILD",
    remote = "https://github.com/bombela/backward-cpp.git",
    tag = "v1.6",
)


http_archive(
    name = "elfutils",
    url = "https://sourceware.org/elfutils/ftp/0.188/elfutils-0.188.tar.bz2",
    sha256 = "fb8b0e8d0802005b9a309c60c1d8de32dd2951b56f0c3a3cb56d21ce01595dff",
    strip_prefix = "elfutils-0.188",
    build_file = "//third_party:elfutils.BUILD",
)

new_git_repository(
    name = "rapidyaml",
    remote = "https://github.com/biojppm/rapidyaml.git",
    build_file = "//third_party:rapidyaml.BUILD",
    tag = "v0.5.0",
    recursive_init_submodules = True,
)



new_git_repository(
    name = "variadic_table",
    remote = "https://github.com/friedmud/variadic_table.git",
    build_file = "//third_party:variadic_table.BUILD",
    commit = "70ddeb66f000a351b36b5a8d04b0820dfeda129f",
)

new_git_repository(
    name = "yamlcpp",
    remote = "https://github.com/jbeder/yaml-cpp.git",
    build_file = "//third_party:yamlcpp.BUILD",
    tag = "yaml-cpp-0.7.0",
)

new_git_repository(
    name = "tabulate",
    remote = "https://github.com/p-ranav/tabulate.git",
    build_file = "//third_party:tabulate.BUILD",
    tag = "v1.5",
)

http_archive(
    name = "libpng",
    url = "https://downloads.sourceforge.net/project/libpng/libpng16/1.6.39/libpng-1.6.39.tar.gz",
    sha256 = "af4fb7f260f839919e5958e5ab01a275d4fe436d45442a36ee62f73e5beb75ba",
    strip_prefix = "libpng-1.6.39",
    build_file = "//third_party:libpng.BUILD",
)

http_archive(
    name = "sdl2",
    url = "https://github.com/libsdl-org/SDL/releases/download/release-2.26.3/SDL2-2.26.3.tar.gz",
    strip_prefix = "SDL2-2.26.3",
    build_file = "//third_party:sdl.BUILD",
)

new_git_repository(
    name = "zlib",
    remote = "https://github.com/madler/zlib.git",
    build_file = "//third_party:zlib.BUILD",
    tag = "v1.2.13",
)

new_git_repository(
    name = "sdl3",
    remote = "https://github.com/libsdl-org/SDL.git",
    build_file = "//third_party:sdl3.BUILD",
    branch = "main",
)

# new_git_repository(
#     name = "libargp",
#     remote = "https://github.com/alexreg/libargp.git",
#     build_file = "//third_party:libargp.BUILD",
#     commit = "d111f205e32d78cf73a9a9997092c0432ef9e895",
#     shallow_since = "1419207665 +0000",
# )


http_archive(
    name = "libargp",
    url = "https://github.com/alexreg/libargp/archive/refs/heads/master.zip",
    strip_prefix = "libargp-master",
    build_file = "//third_party:libargp.BUILD",
)

new_git_repository(
    name = "eigen",
    remote = "https://gitlab.com/libeigen/eigen.git",
    build_file = "//third_party:eigen.BUILD",
    tag = "3.4.0",
)

new_git_repository(
    name = "opencv",
    remote = "https://github.com/opencv/opencv.git",
    build_file = "//third_party:opencv.BUILD",
    tag = "4.7.0",
)


# Can't seem to get get the build rule to find the built dylib
new_git_repository(
    name = "openpnp-capture",
    remote = "https://github.com/openpnp/openpnp-capture.git",
    build_file = "//third_party:openpnp-capture.BUILD",
    tag = "v0.0.24",
)

# Library used in opengl tutorial for textures (//projects/examples/opengl_totorial)
new_git_repository(
    name = "stb",
    remote = "https://github.com/nothings/stb.git",
    build_file = "//third_party:stb.BUILD",
    commit = "f4a71b13373436a2866c5d68f8f80ac6f0bc1ffe", # latest commit for master on 01.19.2024
)

new_git_repository(
    name = "glm",
    remote = "https://github.com/g-truc/glm.git",
    build_file = "//third_party:glm.BUILD",
    branch = "master", # latest commit for master on 01.19.2024
)