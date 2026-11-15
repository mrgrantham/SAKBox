load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@rules_python//python:defs.bzl", "py_binary", "py_test")
load(":texture_recipe.bzl", "texture_recipe")

# Textures drawn on the device from recipes: SVG drawings turned into filled
# outlines at build time (see README.md)
cc_library(
    name = "texture_recipe",
    srcs = ["src/TextureRecipe.cpp"],
    hdrs = ["include/ScratchPadd/TextureRecipe/TextureRecipe.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//visibility:public"],
)

# SVG to recipe, run by the texture_recipe rule (texture_recipe.bzl)
py_binary(
    name = "svg_to_recipe",
    srcs = ["tools/svg_to_recipe.py"],
    visibility = ["//visibility:public"],
)

# The sources, for the consoles' Makefile builds
filegroup(
    name = "sources",
    srcs = glob(["include/**", "src/**"]),
    visibility = ["//visibility:public"],
)

py_test(
    name = "svg_to_recipe_test",
    size = "small",
    srcs = ["tests/svg_to_recipe_test.py"],
    deps = [":svg_to_recipe"],
)

# A drawing turned into a recipe by the build, for the test (and as an example)
texture_recipe(
    name = "example_tile",
    svg = "examples/tile.svg",
    namespace = "TextureRecipeExamples",
    symbol = "kTile",
)

cc_test(
    name = "TextureRecipe_test",
    size = "small",
    srcs = ["tests/TextureRecipe_test.cpp"],
    deps = [
        ":example_tile",
        ":texture_recipe",
        "@gtest//:gtest_main",
    ],
)
