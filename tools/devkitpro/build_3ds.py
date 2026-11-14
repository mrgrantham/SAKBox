"""Builds a 3DS homebrew app with devkitPro's Makefile inside the pinned
devkitARM image: the .3dsx (for the Homebrew Launcher), its .smdh and .elf,
and, given the CIA tools, a .cia (installed to the HOME Menu).

Usage: build_3ds.py IMAGE NAME OUT_DIR --font STB_EASY_FONT_H [--icon PNG]
[--icon-letters TEXT] [--debug] [CIA options] -- [SRC PATH]...
(each source file, then the path it goes at in the app's folder; --debug
runs make with SAKBOX_DEBUG=1)

CIA options: --makerom FILE --bannertool FILE --rsf FILE --logo FILE
--unique-id ID --product-code CODE [--banner-image PNG] [--banner-sound WAV]

Without --icon, the icon is --icon-letters (default: the first two of the
Makefile's APP_TITLE); without --banner-image, the banner shows APP_TITLE,
APP_DESCRIPTION and APP_AUTHOR (see app_images.py).
"""

import argparse
import math
import pathlib
import shlex
import struct
import sys
import tempfile

import app_images
import devkitpro_image

CIA_FOLDER = "_cia"  # in the app's folder: the CIA tools and their inputs
ICON = "_icon/icon.png"  # in the app's folder: the icon, for make's ICON
ICON_SIZE = 48


def default_banner_sound(path):
    """The banner's sound: a soft two-note chime, one second, as a WAV."""
    rate = 22050
    samples = []
    for i in range(rate):
        t = i / rate
        note = 659.25 if t < 0.25 else 880.0
        start = 0.0 if t < 0.25 else 0.25
        envelope = min(1.0, (t - start) * 200) * math.exp(-6 * (t - start))
        samples.append(int(9000 * envelope * math.sin(2 * math.pi * note * (t - start))))
    data = struct.pack(f"<{len(samples)}h", *samples)
    path.write_bytes(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
                     struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" +
                     struct.pack("<I", len(data)) + data)


def cia_commands(name, args):
    """The shell commands that make NAME.cia from the built .elf and .smdh."""
    tools = CIA_FOLDER
    title = shlex.quote(name[:8])  # (the CIA's internal name holds 8 characters)
    settings = {
        "APP_TITLE": title,
        "APP_PRODUCT_CODE": shlex.quote(args.product_code),
        "APP_UNIQUE_ID": args.unique_id,
        "APP_CATEGORY": "Application",
        "APP_USE_ON_SD": "true",
        "APP_ENCRYPTED": "false",
        "APP_MEMORY_TYPE": "Application",
        "APP_SYSTEM_MODE": "64MB",
        "APP_SYSTEM_MODE_EXT": "Legacy",
        "APP_CPU_SPEED": "268MHz",
        "APP_ENABLE_L2_CACHE": "false",
        "APP_VERSION_MAJOR": "1",
    }
    return [
        f"{tools}/bannertool makebanner -i {tools}/banner.png -a {tools}/banner.wav "
        f"-o {tools}/banner.bnr >&2",
        f"{tools}/makerom -f cia -o {shlex.quote(name)}.cia -elf {shlex.quote(name)}.elf "
        f"-rsf {tools}/template.rsf -target t -exefslogo -logo {tools}/logo.bcma.lz "
        f"-icon {shlex.quote(name)}.smdh -banner {tools}/banner.bnr -major 1 -minor 0 -micro 0 " +
        " ".join(f"-D{key}={value}" for key, value in settings.items()) + " >&2",
    ]


def make_defines(defines):
    """`make`'s SAKBOX_DEFINES for `defines` (NAME or NAME=VALUE): -DNAME for
    each, which the app's Makefile adds to its CFLAGS. Nothing for none."""
    if not defines:
        return ""
    return " SAKBOX_DEFINES=" + shlex.quote(" ".join("-D" + define for define in defines))


def main(argv):
    if "--" not in argv:
        sys.exit(__doc__)
    split = argv.index("--")
    parser = argparse.ArgumentParser()
    parser.add_argument("image")
    parser.add_argument("name")
    parser.add_argument("out_dir")
    for option in ["makerom", "bannertool", "rsf", "logo", "unique-id", "product-code", "font",
                   "banner-image", "banner-sound", "icon", "icon-letters"]:
        parser.add_argument("--" + option)
    parser.add_argument("--debug", action="store_true")  # make with SAKBOX_DEBUG=1
    # NAME or NAME=VALUE, each made -DNAME in SAKBOX_DEFINES for make
    parser.add_argument("--define", action="append", default=[])
    args = parser.parse_args(argv[:split])
    files = devkitpro_image.pairs(argv[split + 1:])
    if len(files) * 2 != len(argv) - split - 1:
        sys.exit(__doc__)
    name = args.name

    outputs = [f"{name}{ext}" for ext in (".3dsx", ".smdh", ".elf")]
    commands = [f"make TARGET={shlex.quote(name)} ICON={ICON}" +
                (" SAKBOX_DEBUG=1" if args.debug else "") + make_defines(args.define) + " >&2"]
    font = app_images.EasyFont(pathlib.Path(args.font).read_text())
    makefile = next(source for source, path in files if path == "Makefile")
    text = app_images.makefile_settings(pathlib.Path(makefile).read_text())
    title = text.get("APP_TITLE", name)
    with tempfile.TemporaryDirectory() as scratch:
        icon = args.icon
        if not icon:
            icon = pathlib.Path(scratch, "icon.png")
            letters = args.icon_letters or app_images.icon_letters(title)
            app_images.write_png(app_images.icon(font, letters, ICON_SIZE, title), icon)
        files.append((str(icon), ICON))
        if args.makerom:
            image = args.banner_image
            sound = args.banner_sound
            if not image:
                image = pathlib.Path(scratch, "banner.png")
                app_images.write_png(app_images.banner(font, title,
                                                       text.get("APP_DESCRIPTION", ""),
                                                       text.get("APP_AUTHOR", "")), image)
            if not sound:
                sound = pathlib.Path(scratch, "banner.wav")
                default_banner_sound(sound)
            files += [(args.makerom, f"{CIA_FOLDER}/makerom"),
                      (args.bannertool, f"{CIA_FOLDER}/bannertool"),
                      (args.rsf, f"{CIA_FOLDER}/template.rsf"),
                      (args.logo, f"{CIA_FOLDER}/logo.bcma.lz"),
                      (str(image), f"{CIA_FOLDER}/banner.png"),
                      (str(sound), f"{CIA_FOLDER}/banner.wav")]
            commands += cia_commands(name, args)
            outputs.append(f"{name}.cia")
        commands.append("tar -c " + " ".join(shlex.quote(o) for o in outputs))
        devkitpro_image.run(args.image, files, " && ".join(commands), outputs, args.out_dir,
                            "The devkitPro build")


if __name__ == "__main__":
    main(sys.argv[1:])
