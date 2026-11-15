"""texture_recipe: an SVG drawing turned into a texture recipe at build time,
and compiled into the program (see README.md)."""

load("//tools/embed_file:embed_file.bzl", "embed_file")

def texture_recipe(name, svg, namespace, symbol, **kwargs):
    """A cc_library defining `namespace::symbol` (the recipe's text, from
    `svg`) and `namespace::symbolSize`. Read it with TextureRecipe::read:

        namespace NAMESPACE {
        extern const unsigned char SYMBOL[];
        extern const std::size_t SYMBOLSize;
        }
        auto recipe = ScratchPadd::TextureRecipe::read(
            std::string_view(reinterpret_cast<const char *>(SYMBOL), SYMBOLSize));

    The drawing is checked as it's converted: anything the converter can't
    draw fails the build."""
    native.genrule(
        name = name + "_recipe",
        srcs = [svg],
        outs = [name + ".recipe"],
        cmd = "$(location //projects/scratchpadd/texture_recipe:svg_to_recipe) $< $@",
        tools = ["//projects/scratchpadd/texture_recipe:svg_to_recipe"],
    )
    embed_file(
        name = name,
        src = name + ".recipe",
        namespace = namespace,
        symbol = symbol,
        **kwargs
    )
