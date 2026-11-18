load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("//projects/scratchpadd/texture_recipe:texture_recipe.bzl", "texture_recipe")

# Cosmic Hex's tile faces, drawn as SVG (the designs agreed in rounds 75 to
# 79): planets face down and by kind, the home world's two sides (its city
# side one for each resource it can give), and the deep-space zone's finds.
# The build turns each into a texture recipe; Art.hpp gives a face's recipe,
# which the game draws into a texture when it starts.

texture_recipe(
    name = "find_face_down",
    svg = "find_face_down.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kFindFaceDown",
)

texture_recipe(
    name = "find_nothing",
    svg = "find_nothing.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kFindNothing",
)

texture_recipe(
    name = "find_outpost",
    svg = "find_outpost.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kFindOutpost",
)

texture_recipe(
    name = "find_pirates",
    svg = "find_pirates.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kFindPirates",
)

texture_recipe(
    name = "find_ruins",
    svg = "find_ruins.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kFindRuins",
)

texture_recipe(
    name = "find_salvage",
    svg = "find_salvage.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kFindSalvage",
)

texture_recipe(
    name = "home",
    svg = "home.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kHome",
)

texture_recipe(
    name = "home_exhausted_crystals",
    svg = "home_exhausted_crystals.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kHomeExhaustedCrystals",
)

texture_recipe(
    name = "home_exhausted_food",
    svg = "home_exhausted_food.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kHomeExhaustedFood",
)

texture_recipe(
    name = "home_exhausted_fuel",
    svg = "home_exhausted_fuel.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kHomeExhaustedFuel",
)

texture_recipe(
    name = "home_exhausted_minerals",
    svg = "home_exhausted_minerals.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kHomeExhaustedMinerals",
)

texture_recipe(
    name = "home_exhausted_water",
    svg = "home_exhausted_water.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kHomeExhaustedWater",
)

texture_recipe(
    name = "planet_crystals",
    svg = "planet_crystals.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetCrystals",
)

texture_recipe(
    name = "planet_face_down",
    svg = "planet_face_down.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetFaceDown",
)

texture_recipe(
    name = "planet_food",
    svg = "planet_food.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetFood",
)

texture_recipe(
    name = "planet_fuel",
    svg = "planet_fuel.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetFuel",
)

texture_recipe(
    name = "planet_minerals",
    svg = "planet_minerals.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetMinerals",
)

texture_recipe(
    name = "planet_moon",
    svg = "planet_moon.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetMoon",
)

texture_recipe(
    name = "planet_water",
    svg = "planet_water.svg",
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kPlanetWater",
)

# The planets' number chips (round 87): a number on a round chip with a dot
# for each way two dice make it; the SVGs are written by make_number_chips.py
CHIP_NUMBERS = [2, 3, 4, 5, 6, 8, 9, 10, 11, 12]

[texture_recipe(
    name = "number_%d" % n,
    svg = "number_%d.svg" % n,
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kNumber%d" % n,
) for n in CHIP_NUMBERS]

# The board screen's buttons' icons (round 94: SVG files, to swap for your
# own drawings), each drawn on a transparent square
ICONS = ["roll", "build", "trade", "show_resources", "cards", "end_turn"]

[texture_recipe(
    name = "icon_" + name,
    svg = "icons/%s.svg" % name,
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kIcon" + "".join([word.capitalize() for word in name.split("_")]),
) for name in ICONS]

# The cards' art (round 75's designs, round 94: no words, which the game
# writes over them, and a title band for two lines), one for each card
CARDS = ["resource_minerals", "resource_food", "resource_fuel", "resource_water", "resource_crystals", "enforcer", "interstellar_cartel", "shipyard_surplus", "supply_drop", "charter_three_kinds", "charter_occupier", "charter_deep_space_relay"]

[texture_recipe(
    name = "card_" + name,
    svg = "cards/%s.svg" % name,
    namespace = "CosmicHex::Art::Recipes",
    symbol = "kCard" + "".join([word.capitalize() for word in name.split("_")]),
) for name in CARDS]

cc_library(
    name = "art",
    srcs = ["src/Art.cpp"],
    hdrs = ["include/CosmicHex/Art.hpp"],
    strip_include_prefix = "include/",
    visibility = ["//projects/cosmic_hex:__subpackages__"],
    deps = [
        ":find_face_down",
        ":find_nothing",
        ":find_outpost",
        ":find_pirates",
        ":find_ruins",
        ":find_salvage",
        ":home",
        ":home_exhausted_crystals",
        ":home_exhausted_food",
        ":home_exhausted_fuel",
        ":home_exhausted_minerals",
        ":home_exhausted_water",
        ":planet_crystals",
        ":planet_face_down",
        ":planet_food",
        ":planet_fuel",
        ":planet_minerals",
        ":planet_moon",
        ":planet_water",
    ] + [":number_%d" % n for n in CHIP_NUMBERS] +
    [":icon_" + name for name in ICONS] + [":card_" + name for name in CARDS],
)

# The sources, for the consoles' Makefile builds: Art.cpp and the recipes'
# generated C++ (Art.hpp from include/)
filegroup(
    name = "sources",
    srcs = [
        "include/CosmicHex/Art.hpp",
        "src/Art.cpp",
        ":find_face_down_source",
        ":find_nothing_source",
        ":find_outpost_source",
        ":find_pirates_source",
        ":find_ruins_source",
        ":find_salvage_source",
        ":home_source",
        ":home_exhausted_crystals_source",
        ":home_exhausted_food_source",
        ":home_exhausted_fuel_source",
        ":home_exhausted_minerals_source",
        ":home_exhausted_water_source",
        ":planet_crystals_source",
        ":planet_face_down_source",
        ":planet_food_source",
        ":planet_fuel_source",
        ":planet_minerals_source",
        ":planet_moon_source",
        ":planet_water_source",
    ] + [":number_%d_source" % n for n in CHIP_NUMBERS] +
    [":icon_%s_source" % name for name in ICONS] + [":card_%s_source" % name for name in CARDS],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "Art_test",
    size = "small",
    srcs = ["tests/Art_test.cpp"],
    deps = [
        ":art",
        "//projects/scratchpadd/texture_recipe",
        "@gtest//:gtest_main",
    ],
)
