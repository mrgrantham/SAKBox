"""Nintendo 3DS homebrew built with devkitPro, pinned and run by Bazel.

devkitPro's toolchain (devkitARM, libctru, citro2d/3d, 3dstools, the 3ds-*
portlibs) comes from devkitPro's official Docker image, pinned below by
digest, so every build uses exactly the same compiler and libraries. To
update, pick a dated tag from https://hub.docker.com/r/devkitpro/devkitarm/tags,
`docker pull devkitpro/devkitarm:<tag>` and copy its digest here.

The image holds Linux programs, so building needs a container runtime (on
macOS: `brew install colima docker`, then `colima start`). The apps it makes
run on a 3DS (via the Homebrew Launcher) or in an emulator.
"""

# devkitpro/devkitarm:20260610 (devkitARM r68, GCC 16.1, libctru 2.7.0,
# citro2d 1.7.0, citro3d 1.7.1, 3dstools 1.3.1), linux/arm64 and amd64
DEVKITARM_IMAGE = "devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5"

def _devkitpro_3ds_app_impl(ctx):
    name = ctx.attr.app_name or ctx.label.name
    outs = [ctx.actions.declare_file(name + ext) for ext in [".3dsx", ".smdh", ".elf"]]
    pkg = ctx.label.package
    args = ctx.actions.args()
    args.add(DEVKITARM_IMAGE)
    args.add(name)
    args.add(outs[0].dirname)
    args.add(pkg)
    args.add_all(ctx.files.srcs)
    ctx.actions.run(
        executable = ctx.executable._build,
        arguments = [args],
        inputs = ctx.files.srcs,
        outputs = outs,
        mnemonic = "DevkitPro3ds",
        progress_message = "Building 3DS app %{label} with devkitPro",
        # Talks to the Docker daemon, outside Bazel's sandbox.
        execution_requirements = {"no-sandbox": "1", "no-remote": "1"},
        use_default_shell_env = True,
    )
    return [DefaultInfo(files = depset(outs))]

devkitpro_3ds_app = rule(
    implementation = _devkitpro_3ds_app_impl,
    doc = "Builds a 3DS homebrew app (.3dsx, .smdh, .elf) with devkitPro's " +
          "standard 3DS Makefile, which must be among `srcs` with the " +
          "sources at the paths it expects (source/, include/, ...).",
    attrs = {
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "app_name": attr.string(doc = "Output name (default: the target name)"),
        "_build": attr.label(
            default = "//tools/devkitpro:build_3ds.sh",
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
)
