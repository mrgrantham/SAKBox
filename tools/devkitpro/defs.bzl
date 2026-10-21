"""Nintendo 3DS homebrew built with devkitPro, pinned and run by Bazel.

devkitPro's toolchain (devkitARM, libctru, citro2d/3d, 3dstools, the 3ds-*
portlibs) comes from devkitPro's official Docker image, pinned below by
digest, so every build uses exactly the same compiler and libraries. To
update, pick a dated tag from https://hub.docker.com/r/devkitpro/devkitarm/tags,
`docker pull devkitpro/devkitarm:<tag>` and copy its digest here.

The image holds Linux programs, so building needs a container runtime (on
macOS: `brew install colima docker`, then `colima start`). The apps it makes
run on a 3DS (via the Homebrew Launcher) or in an emulator. The image is
built for x86-64 and ARM, and the build step is Python, so this works on
macOS, Linux and Windows hosts alike.
"""

# devkitpro/devkitarm:20260610 (devkitARM r68, GCC 16.1, libctru 2.7.0,
# citro2d 1.7.0, citro3d 1.7.1, 3dstools 1.3.1), linux/arm64 and amd64
DEVKITARM_IMAGE = "devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5"

def _staged_under(file, root):
    """`file`'s path below `root` (a package's or repository's folder)."""
    prefix = root + "/" if root else ""
    if not file.path.startswith(prefix):
        fail("%s isn't under %s; only source files can be staged" % (file.path, root))
    return file.path[len(prefix):]

def _package_root(label):
    return "/".join([part for part in [label.workspace_root, label.package] if part])

def _devkitpro_3ds_app_impl(ctx):
    name = ctx.attr.app_name or ctx.label.name
    outs = [ctx.actions.declare_file(name + ext) for ext in [".3dsx", ".smdh", ".elf"]]

    # Each file and where it goes in the app's folder: `srcs` at their paths
    # in this package, `extra_srcs` under their folder at their paths in
    # their own package (or repository)
    staged = [(f, _staged_under(f, _package_root(ctx.label))) for f in ctx.files.srcs]
    for target, folder in ctx.attr.extra_srcs.items():
        root = _package_root(target.label)
        for f in target.files.to_list():
            staged.append((f, folder.rstrip("/") + "/" + _staged_under(f, root)))

    args = ctx.actions.args()
    args.add(DEVKITARM_IMAGE)
    args.add(name)
    args.add(outs[0].dirname)
    for f, path in staged:
        args.add(f)
        args.add(path)
    ctx.actions.run(
        executable = ctx.executable._build,
        arguments = [args],
        inputs = [f for f, _ in staged],
        outputs = outs,
        mnemonic = "DevkitPro3ds",
        progress_message = "Building 3DS app %{label} with devkitPro",
        # Talks to the Docker daemon, outside Bazel's sandbox.
        execution_requirements = {"no-sandbox": "1", "no-remote": "1"},
        use_default_shell_env = True,
    )
    return [DefaultInfo(files = depset(outs))]

_devkitpro_3ds_app = rule(
    implementation = _devkitpro_3ds_app_impl,
    doc = "Builds a 3DS homebrew app (.3dsx, .smdh, .elf) with devkitPro's " +
          "standard 3DS Makefile, which must be among `srcs` with the " +
          "sources at the paths it expects (source/, include/, ...).",
    attrs = {
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "extra_srcs": attr.label_keyed_string_dict(
            allow_files = True,
            doc = "Files from other packages or repositories, each label " +
                  "mapped to the folder its files go in, at their paths " +
                  "within their own package: e.g. {\"@miniaudio//:sources\": " +
                  "\"third_party/miniaudio\"}.",
        ),
        "app_name": attr.string(doc = "Output name (default: the target name)"),
        "_build": attr.label(
            default = "//tools/devkitpro:build_3ds",
            executable = True,
            cfg = "exec",
        ),
    },
)

def devkitpro_3ds_app(name, tags = [], **kwargs):
    """A 3DS app (see _devkitpro_3ds_app), tagged `devkitpro`.

    Builds that can't run Docker (tools/linux_check, inside its container)
    leave every 3DS app out with --build_tag_filters=-devkitpro.
    """
    _devkitpro_3ds_app(name = name, tags = tags + ["devkitpro"], **kwargs)
