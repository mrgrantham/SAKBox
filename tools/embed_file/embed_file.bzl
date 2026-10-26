"""embed_file: a file's bytes compiled into a C++ program (see embed_file.py)."""

load("@rules_cc//cc:defs.bzl", "cc_library")

def embed_file(name, src, namespace, symbol, **kwargs):
    """A cc_library defining `namespace::symbol` (the bytes of `src`) and
    `namespace::symbolSize`. Declare them where they're used:

        namespace NAMESPACE {
        extern const unsigned char SYMBOL[];
        extern const std::size_t SYMBOLSize;
        }
    """
    native.genrule(
        name = name + "_source",
        srcs = [src],
        outs = [name + ".cpp"],
        cmd = "$(location //tools/embed_file) $< $@ {} {}".format(namespace, symbol),
        tools = ["//tools/embed_file"],
    )
    cc_library(
        name = name,
        srcs = [name + ".cpp"],
        **kwargs
    )
