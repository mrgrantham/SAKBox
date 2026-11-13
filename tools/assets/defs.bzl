"""Models from Blender for ScratchPadd's scene layer (see README.md).

scene_model exports a .blend with Blender (//tools/blender), then cooks it
for each platform: the desktop's version as it is, the DS's and the 3DS's
checked against their budgets and simplified to fit if the rule says so,
each as C++ (a Scene::MeshData). scene adds up what the models it lists cost
a frame on the DS and the 3DS, and fails the build if it's over.
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_cc//cc:defs.bzl", "cc_library")

ModelInfo = provider(
    doc = "What a cooked model costs to draw on each platform.",
    fields = {"counts": "Files, one a platform: a line with its name, polygons and vertices"},
)

# What each platform draws a frame at most (0: no limit). The DS's are its
# hardware's: 2048 polygons and 6144 vertices a frame (GBATEK). The 3DS's
# GPU has no fixed limit, only its speed: 45,000 triangles is about what a
# New 3DS draws lit at 60 fps in stereo, a few pixels each (measured with
# //projects/scratchpadd/scene/3ds_benchmark); large ones fill fewer, which
# only drawing shows (Renderer::lastFrameTimes).
FRAME_LIMITS = {
    "desktop": struct(polygons = 0, vertices = 0),
    "ds": struct(polygons = 2048, vertices = 6144),
    "3ds": struct(polygons = 45000, vertices = 0),
}

_MODES = ["FAIL", "SIMPLIFY", "SIMPLIFY_AGGRESSIVE"]

# The generated files for each output: (platform, suffix, namespace suffix)
_OUTPUTS = [
    ("desktop", "", ""),
    ("ds", "_ds", ""),
    ("ds", "_ds_preview", "::Ds"),
    ("3ds", "_3ds", ""),
]

def _scene_model_impl(ctx):
    name = ctx.attr.model_name

    # Export the .blend as glTF, with Bazel's Blender (or BLENDER)
    glb = ctx.actions.declare_file(ctx.label.name + ".glb")
    blender = ctx.attr._blender[DefaultInfo]
    programs = blender.files.to_list()
    ctx.actions.run(
        executable = ctx.file._run_blender,
        arguments = [
            programs[0].path if programs else "",
            ctx.file.src.path,
            "--python",
            ctx.file._export.path,
            "--",
            glb.path,
        ],
        inputs = depset([ctx.file.src, ctx.file._export], transitive = [blender.files, blender.default_runfiles.files]),
        outputs = [glb],
        mnemonic = "BlenderExport",
        progress_message = "Exporting %{input} from Blender",
        use_default_shell_env = True,  # BLENDER, with --action_env
    )

    # Cook it for each platform, each in an action of its own, so a model
    # over the DS's budget stops only what's built for the DS
    override = ctx.attr._over_budget[BuildSettingInfo].value
    groups = {}
    reports = []
    counts = []
    for platform, suffix, namespace in _OUTPUTS:
        limit = FRAME_LIMITS[platform]
        polygons = ctx.attr.budget.get(platform, str(limit.polygons))
        mode = override or ctx.attr.over_budget.get(platform, "FAIL")
        if mode not in _MODES:
            fail("over_budget for {} is {}: it must be one of {}".format(platform, mode, _MODES))
        base = name + suffix
        cc = ctx.actions.declare_file(base + ".cpp")  # (devkitPro's Makefiles build .cpp, not .cc)
        h = ctx.actions.declare_file(base + ".h")
        report = ctx.actions.declare_file(base + ".report.json")
        count = ctx.actions.declare_file(base + ".counts")
        args = ctx.actions.args()
        args.add_all(["model", "--gltf", glb, "--name", name, "--object", ctx.attr.object])
        args.add_all(["--label", str(ctx.label), "--report", report, "--counts", count])
        if not ctx.attr.cleanup:
            args.add("--no-cleanup")
        args.add_all(["--output", platform, ctx.attr.namespace + namespace, cc, h])
        args.add_all([polygons, str(limit.vertices), mode])
        ctx.actions.run(
            executable = ctx.executable._cook,
            arguments = [args],
            inputs = [glb],
            outputs = [cc, h, report, count],
            mnemonic = "CookModel",
            progress_message = "Cooking %{label} for " + platform + suffix.replace("_", " "),
        )
        group = suffix[1:] + "_" if suffix else ""
        groups[group + "srcs"] = depset([cc])
        groups[group + "hdrs"] = depset([h])
        reports.append(report)
        if suffix != "_ds_preview":
            counts.append(count)
    return [
        DefaultInfo(files = depset(reports)),
        OutputGroupInfo(report = depset(reports), glb = depset([glb]), **groups),
        ModelInfo(counts = counts),
    ]

_scene_model = rule(
    implementation = _scene_model_impl,
    attrs = {
        "src": attr.label(allow_single_file = [".blend"], mandatory = True),
        "object": attr.string(mandatory = True),
        "model_name": attr.string(mandatory = True),
        "namespace": attr.string(default = "Assets"),
        "budget": attr.string_dict(),  # filled in by the macro, as ints
        "over_budget": attr.string_dict(),
        "cleanup": attr.bool(default = True),
        "_blender": attr.label(default = "//tools/blender:program"),
        "_run_blender": attr.label(default = "//tools/blender:run_blender.sh", allow_single_file = True),
        "_export": attr.label(default = "//tools/assets:export_gltf.py", allow_single_file = True),
        "_cook": attr.label(default = "//tools/assets:cook", executable = True, cfg = "exec"),
        "_over_budget": attr.label(default = "//tools/assets:over_budget"),
    },
)

def scene_model(name, src, object, budget = {}, over_budget = {}, cleanup = True, namespace = "Assets", tags = [], **kwargs):
    """A model from Blender, cooked for each platform.

    Makes:
      NAME: a cc_library for desktops: NAME.h, with `NAMESPACE::NAME`, a
        Scene::MeshData, and NAMETriangles, NAMEVertices and NAMEPolygons.
      NAME_ds_sources: the DS's version (the same names), for a DS app's
        Makefile build (devkitpro_nds_app's extra_srcs).
      NAME_ds_preview: a cc_library of the DS's version for desktops, in
        NAMESPACE::Ds, to see what the DS draws.
      NAME_3ds_sources: the 3DS's version (the same names), for a 3DS app's
        Makefile build (devkitpro_3ds_app's extra_srcs).
      NAME_cooked: the files, and the report (output group "report").

    Args:
      name: the model's name, in C++ too.
      src: the .blend.
      object: the object in it to use (its platform versions are found
        beside it: OBJECT_DS or OBJECT_3DS, or sak.platform = "ds" or
        "3ds").
      budget: polygons a platform's version may use, by platform ("ds",
        "3ds": the 3DS counts triangles); its frame limit if not given.
      over_budget: by platform, what to do over budget: "FAIL" (the
        default), "SIMPLIFY" or "SIMPLIFY_AGGRESSIVE".
        --//tools/assets:over_budget=MODE overrides them all for a build.
      cleanup: whether each platform's version is cleaned up first
        (triangles on flat surfaces merged where it looks no different):
        False keeps every triangle as made, for a model heavy on purpose
        (to test what a device draws).
      namespace: the C++ namespace for the model.
      tags: added to each target's, with "blender" (the Linux check's ARM64
        runs leave those out: there's no Blender for Linux on ARM).
    """
    tags = tags + ["blender"]
    _scene_model(
        name = name + "_cooked",
        src = src,
        object = object,
        model_name = name,
        namespace = namespace,
        budget = {platform: str(polygons) for platform, polygons in budget.items()},
        over_budget = over_budget,
        cleanup = cleanup,
        tags = tags,
        **kwargs
    )
    for group, label in [("srcs", "srcs"), ("hdrs", "hdrs"), ("ds_srcs", "ds_srcs"), ("ds_hdrs", "ds_hdrs"), ("ds_preview_srcs", "ds_preview_srcs"), ("ds_preview_hdrs", "ds_preview_hdrs"), ("3ds_srcs", "3ds_srcs"), ("3ds_hdrs", "3ds_hdrs")]:
        native.filegroup(name = name + "_" + label, srcs = [":" + name + "_cooked"], output_group = group, tags = tags, **kwargs)
    for library, prefix in [(name, ""), (name + "_ds_preview", "ds_preview_")]:
        cc_library(
            name = library,
            srcs = [":" + name + "_" + prefix + "srcs"],
            hdrs = [":" + name + "_" + prefix + "hdrs"],
            includes = ["."],
            deps = ["//projects/scratchpadd/scene"],
            tags = tags,
            **kwargs
        )
    for platform in ["ds", "3ds"]:
        native.filegroup(
            name = name + "_" + platform + "_sources",
            srcs = [":" + name + "_" + platform + "_srcs", ":" + name + "_" + platform + "_hdrs"],
            tags = tags,
            **kwargs
        )

def _scene_impl(ctx):
    report = ctx.actions.declare_file(ctx.label.name + ".scene.txt")
    args = ctx.actions.args()
    args.add_all(["scene", "--label", str(ctx.label), "--report", report])
    for platform, limit in FRAME_LIMITS.items():
        if limit.polygons:
            args.add_all(["--limit", platform, str(limit.polygons), str(limit.vertices)])
    counts = []
    for model, how_many in ctx.attr.models.items():
        for file in model[ModelInfo].counts:
            counts.append(file)
            args.add_all(["--model", file, how_many])
    ctx.actions.run(
        executable = ctx.executable._cook,
        arguments = [args],
        inputs = counts,
        outputs = [report],
        mnemonic = "CheckScene",
        progress_message = "Adding up what %{label} draws a frame",
    )
    return [
        DefaultInfo(files = depset([report])),
        OutputGroupInfo(_validation = depset([report])),
    ]

_scene = rule(
    implementation = _scene_impl,
    attrs = {
        "models": attr.label_keyed_string_dict(providers = [ModelInfo]),
        "_cook": attr.label(default = "//tools/assets:cook", executable = True, cfg = "exec"),
    },
)

def scene(name, models, tags = [], **kwargs):
    """Everything a scene shows at once, checked against each platform's frame limit.

    Building it (or anything that depends on it) fails if the models, each
    times how many are shown at once, cost more than a frame on the DS can
    draw (2048 polygons, 6144 vertices), or the 3DS (45,000 triangles). Its output is a report of each
    platform's total and the costliest models.

    Args:
      name: the scene's name.
      models: each scene_model (its name) and how many are shown at once.
      tags: added to the target's, with "blender".
    """
    _scene(
        name = name,
        models = {model + "_cooked": str(how_many) for model, how_many in models.items()},
        tags = tags + ["blender"],
        **kwargs
    )
