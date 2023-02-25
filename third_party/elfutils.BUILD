load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "source",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

configure_make(
    name = "elfutils",
    configure_in_place = True,
    lib_source = ":source",
    targets = [
        "simple",
        "install",
    ],
    configure_options = ["CC=\"gcc\"","CXX=\"gcc\"","CFLAGS=","AR=\"ar\"","LDFLAGS=","--enable-linker-build-id"],
    visibility = ["//visibility:public"],
    out_static_libs = ["libdw.a","libelf.a"],
    linkopts = ["-largp",],
    # deps = ["@libargp"],

)