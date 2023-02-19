
workspace(name = "graphFun")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "cyclone",
    url = "https://github.com/idmillington/cyclone-physics/archive/refs/heads/master.zip",
    sha256 = "faeda03d28499b7120208d7e70759bf1832511b6d64f546ab32103e9ba400f03",
    strip_prefix = "cyclone-physics-master",
    build_file = "@//third_party:cyclone.BUILD",
)