load("//tools/assets:defs.bzl", "scene_model")

# The title screen's model (round 87): a retro rocket on a pedestal, made by
# make_title_models.py into title_models.blend (a placeholder: open the
# .blend in Blender and change or replace the object named Rocket). The
# build exports and cooks it for each platform (tools/assets).
scene_model(
    name = "rocket",
    src = "title_models.blend",
    # (the DS's whole title screen: the rocket, the title's letters and the
    # stars, within its 2048 polygons a frame)
    budget = {"ds": 500},
    object = "Rocket",
    over_budget = {"ds": "SIMPLIFY"},
    visibility = ["//projects/cosmic_hex:__subpackages__"],
)
