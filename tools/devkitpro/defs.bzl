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

def _devkitpro_cia_tools_impl(ctx):
    outs = [ctx.actions.declare_file(tool) for tool in ["makerom", "bannertool"]]
    args = ctx.actions.args()
    args.add(DEVKITARM_IMAGE)
    args.add(outs[0].dirname)
    inputs = []
    for target, folder in [(ctx.attr._makerom, ""), (ctx.attr._bannertool, "bannertool/")]:
        root = _package_root(target.label)
        for f in target.files.to_list():
            args.add(f)
            args.add(folder + _staged_under(f, root))
            inputs.append(f)
    ctx.actions.run(
        executable = ctx.executable._build,
        arguments = [args],
        inputs = inputs,
        outputs = outs,
        mnemonic = "DevkitProCiaTools",
        progress_message = "Building makerom and bannertool in devkitPro's image",
        execution_requirements = {"no-sandbox": "1", "no-remote": "1"},
        use_default_shell_env = True,
    )
    return [DefaultInfo(files = depset(outs))]

devkitpro_cia_tools = rule(
    implementation = _devkitpro_cia_tools_impl,
    doc = "makerom and bannertool, built from source inside the devkitPro " +
          "image so they run there, for making CIAs (//tools/devkitpro:cia_tools).",
    attrs = {
        "_makerom": attr.label(default = "@project_ctr//:makerom_sources"),
        "_bannertool": attr.label(default = "@bannertool//:sources"),
        "_build": attr.label(
            default = "//tools/devkitpro:build_cia_tools",
            executable = True,
            cfg = "exec",
        ),
    },
)

def _unique_id(ctx):
    """The CIA's unique ID: as given, or worked out from the target's label,
    in the range 0xF0000-0xF7FFF that homebrew uses."""
    if ctx.attr.unique_id:
        return ctx.attr.unique_id
    return "0x%X" % (0xF0000 + abs(hash(str(ctx.label))) % 0x8000)

def _devkitpro_3ds_app_impl(ctx):
    name = ctx.attr.app_name or ctx.label.name
    outs = [ctx.actions.declare_file(name + ext) for ext in [".3dsx", ".smdh", ".elf", ".cia"]]

    # Each file and where it goes in the app's folder: `srcs` at their paths
    # in this package, `extra_srcs` under their folder at their paths in
    # their own package (or repository)
    staged = [(f, _staged_under(f, _package_root(ctx.label))) for f in ctx.files.srcs]
    for target, folder in ctx.attr.extra_srcs.items():
        root = _package_root(target.label)
        for f in target.files.to_list():
            staged.append((f, folder.rstrip("/") + "/" + _staged_under(f, root)))

    # The CIA: the tools, the standard settings and logo, the banner
    unique_id = _unique_id(ctx)
    makerom, bannertool = ctx.files._cia_tools
    cia_inputs = [makerom, bannertool, ctx.file._rsf, ctx.file._logo]
    args = ctx.actions.args()
    args.add(DEVKITARM_IMAGE)
    args.add(name)
    args.add(outs[0].dirname)
    args.add("--makerom", makerom)
    args.add("--bannertool", bannertool)
    args.add("--rsf", ctx.file._rsf)
    args.add("--logo", ctx.file._logo)
    args.add("--unique-id", unique_id)
    args.add("--product-code", ctx.attr.product_code or "CTR-H-" + unique_id[-4:])
    if ctx.file.banner_image:
        args.add("--banner-image", ctx.file.banner_image)
        cia_inputs.append(ctx.file.banner_image)
    if ctx.file.banner_sound:
        args.add("--banner-sound", ctx.file.banner_sound)
        cia_inputs.append(ctx.file.banner_sound)
    args.add("--")
    for f, path in staged:
        args.add(f)
        args.add(path)
    ctx.actions.run(
        executable = ctx.executable._build,
        arguments = [args],
        inputs = [f for f, _ in staged] + cia_inputs,
        outputs = outs,
        mnemonic = "DevkitPro3ds",
        progress_message = "Building 3DS app %{label} with devkitPro",
        # Talks to the Docker daemon, outside Bazel's sandbox.
        execution_requirements = {"no-sandbox": "1", "no-remote": "1"},
        use_default_shell_env = True,
    )

    # `bazel run` opens the app in the Azahar emulator Bazel downloaded
    azahar = ctx.attr._azahar[DefaultInfo]
    azahar_program = azahar.files.to_list()
    launcher = ctx.actions.declare_file(name + "_in_azahar.sh")
    ctx.actions.expand_template(
        template = ctx.file._launcher,
        output = launcher,
        substitutions = {
            "@@APP@@": outs[0].short_path,
            "@@AZAHAR@@": azahar_program[0].short_path if azahar_program else "",
        },
        is_executable = True,
    )
    runfiles = ctx.runfiles(files = [outs[0]] + azahar_program).merge(azahar.default_runfiles)
    return [DefaultInfo(files = depset(outs), executable = launcher, runfiles = runfiles)]

_devkitpro_3ds_app = rule(
    implementation = _devkitpro_3ds_app_impl,
    doc = "Builds a 3DS homebrew app (.3dsx, .cia, .smdh, .elf) with devkitPro's " +
          "standard 3DS Makefile, which must be among `srcs` with the " +
          "sources at the paths it expects (source/, include/, ...). " +
          "`bazel run` opens it in the Azahar emulator.",
    executable = True,
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
        "unique_id": attr.string(
            doc = "The CIA's unique ID, 0xF0000-0xF7FFF for homebrew (default: " +
                  "one worked out from the target's label). Two CIAs with the same " +
                  "ID replace each other when installed.",
        ),
        "product_code": attr.string(doc = "The CIA's product code (default: CTR-H- and the ID)"),
        "banner_image": attr.label(
            allow_single_file = [".png"],
            doc = "The HOME Menu banner: a 256x128 PNG (default: a plain one)",
        ),
        "banner_sound": attr.label(
            allow_single_file = [".wav"],
            doc = "The banner's sound: a WAV of up to 3 seconds (default: a chime)",
        ),
        "_cia_tools": attr.label(default = "//tools/devkitpro:cia_tools"),
        "_rsf": attr.label(default = "@ctr_buildtools//:3ds/template.rsf", allow_single_file = True),
        "_logo": attr.label(default = "@ctr_buildtools//:3ds/logo.bcma.lz", allow_single_file = True),
        "_launcher": attr.label(
            default = "//tools/devkitpro:run_in_azahar.sh",
            allow_single_file = True,
        ),
        "_azahar": attr.label(default = "//tools/azahar"),
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
