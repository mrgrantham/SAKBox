load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

# filegroup(
#     name = "source",
#     srcs = glob(["**"]),
# )
# filessss = glob(["**"])
# print(filessss)
# print("XXXXXXXXXXX")
# old_file_locations = glob(["**"])
# print(old_file_locations)
# # This is the logic for actually remapping the paths. Consider a macro
# # instead of writing it inline like this if it gets more complicated.
# # path[path.find("argp/"):]
# base_file_locations = [path.lstrip("argp/") if path.startswith("argp/") else path for path in old_file_locations]
# print(base_file_locations)
# genrule(
#     name = "source",
#     srcs = old_file_locations,
#     outs = base_file_locations,
#     cmd = "\n".join(["cp $(location %s) $(location %s)" %
#         (src, dest) for src, dest in zip(old_file_locations, base_file_locations)]),
#     visibility = ["//simple_make:__subpackages__"],
# )

# configure_make(
#     name = "libargp",
#     configure_command = "argp/configure",
#     # configure_in_place = True,
#     lib_source = ":source",
#     targets = [""],
#     visibility = ["//visibility:public"],
#     out_static_libs = ["libargp.a"],
# )
locations = glob(["**"])
print(locations)
# actions.write("build", "", is_executable=True)

genrule(
  name = "libargp",
  srcs = locations,
  outs = ["libargp.a"],
    # tools = ["build","common","gnulib/configure"],
  cmd = "$(location build)",
    visibility = ["//visibility:public"],
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)