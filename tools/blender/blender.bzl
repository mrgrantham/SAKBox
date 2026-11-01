"""Blender, downloaded by Bazel for the machine it runs on, to export models.

blender.org publishes Blender for macOS on Apple silicon (a .dmg) and Linux
x86-64 (a .tar.xz), with SHA-256s; each is pinned here. Bazel's own archive
download can't open a .dmg, so on macOS this mounts it (hdiutil) and copies
Blender.app out. On a machine with no Blender build (Intel Macs, Linux on
ARM), the repository has no Blender, and //tools/blender:blender says why
when it's run. BLENDER, set to a Blender program, is used instead on any
machine (as MELONDS is for melonDS). See README.md.
"""

_VERSION = "5.2.3"
_URL = "https://download.blender.org/release/Blender5.2/blender-{v}-{platform}.{ext}"

_DOWNLOADS = {
    "macos_arm64": struct(
        platform = "macos-arm64",
        ext = "dmg",
        sha256 = "acc05374485a272342d4b323c15ac01953e7f697da2f1bad8af9c3bf087267a4",
        program = "Blender.app/Contents/MacOS/Blender",
    ),
    "linux_x86_64": struct(
        platform = "linux-x64",
        ext = "tar.xz",
        sha256 = "78486c018b4443efe5e75ab54c290ed8c15abafd249b862c2c9ce2530a918eec",
        program = "blender-{v}-linux-x64/blender",
    ),
}

_BUILD = """\
# Blender {version}, as blender.org publishes it (see //tools/blender)
filegroup(
    name = "program",
    srcs = ["{program}"],
    data = glob(["{bundle}/**"], allow_empty = True),
    visibility = ["//visibility:public"],
)
"""

_NO_BLENDER_BUILD = """\
# No Blender for this machine: {why} (see //tools/blender)
filegroup(
    name = "program",
    srcs = [],
    visibility = ["//visibility:public"],
)
"""

def _machine(rctx):
    os = rctx.os.name.lower()
    arch = rctx.os.arch.lower()
    if os.startswith("mac") and arch in ("aarch64", "arm64"):
        return "macos_arm64"
    if os.startswith("linux") and arch in ("amd64", "x86_64"):
        return "linux_x86_64"
    return None

def _blender_repository_impl(rctx):
    machine = _machine(rctx)
    if not machine:
        rctx.file("BUILD.bazel", _NO_BLENDER_BUILD.format(
            why = "blender.org has no Blender {} for {} on {}".format(_VERSION, rctx.os.name, rctx.os.arch),
        ))
        return
    download = _DOWNLOADS[machine]
    url = _URL.format(v = _VERSION, platform = download.platform, ext = download.ext)
    program = download.program.format(v = _VERSION)
    if download.ext == "dmg":
        rctx.download(url = url, output = "blender.dmg", sha256 = download.sha256)
        mount = str(rctx.path("mount"))
        attached = rctx.execute(["hdiutil", "attach", "-nobrowse", "-readonly", "-mountpoint", mount, "blender.dmg"])
        if attached.return_code != 0:
            fail("Couldn't open Blender's disk image: " + attached.stderr)

        # An APFS clone where it can (instant, and no extra space), else a copy
        copied = rctx.execute(["sh", "-c", 'cp -cR "$0/Blender.app" . 2>/dev/null || cp -R "$0/Blender.app" .', mount])
        rctx.execute(["hdiutil", "detach", mount])
        rctx.delete("blender.dmg")
        if copied.return_code != 0:
            fail("Couldn't copy Blender.app out of its disk image: " + copied.stderr)
        bundle = "Blender.app"
    else:
        rctx.download_and_extract(url = url, sha256 = download.sha256)
        bundle = program.split("/")[0]
    rctx.file("BUILD.bazel", _BUILD.format(version = _VERSION, program = program, bundle = bundle))

blender_repository = repository_rule(
    implementation = _blender_repository_impl,
    doc = "Blender for the machine Bazel runs on (see //tools/blender).",
)
