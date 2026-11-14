"""Builds a Nintendo DS homebrew app with devkitPro's Makefile inside the
pinned devkitARM image: the .nds (to run on a DS, from a flash card, or in an
emulator) and its .elf.

Usage: build_nds.py IMAGE NAME OUT_DIR --font STB_EASY_FONT_H [--icon BMP]
[--icon-letters TEXT] [--debug] -- [SRC PATH]...
(each source file, then the path it goes at in the app's folder; --debug
runs make with SAKBOX_DEBUG=1)

Without --icon, the icon is --icon-letters (default: the first two of the
Makefile's GAME_TITLE; see app_images.py).
"""

import argparse
import pathlib
import shlex
import sys
import tempfile

import app_images
import devkitpro_image

ICON = "_icon/icon.bmp"  # in the app's folder: the icon, for make's GAME_ICON
ICON_SIZE = 32


def main(argv):
    if "--" not in argv:
        sys.exit(__doc__)
    split = argv.index("--")
    parser = argparse.ArgumentParser()
    parser.add_argument("image")
    parser.add_argument("name")
    parser.add_argument("out_dir")
    for option in ["font", "icon", "icon-letters"]:
        parser.add_argument("--" + option)
    parser.add_argument("--debug", action="store_true")  # make with SAKBOX_DEBUG=1
    # NAME or NAME=VALUE, each made -DNAME in SAKBOX_DEFINES for make
    parser.add_argument("--define", action="append", default=[])
    args = parser.parse_args(argv[:split])
    files = devkitpro_image.pairs(argv[split + 1:])
    if len(files) * 2 != len(argv) - split - 1:
        sys.exit(__doc__)
    name = args.name

    makefile = next(source for source, path in files if path == "Makefile")
    title = app_images.makefile_settings(pathlib.Path(makefile).read_text(),
                                         ("GAME_TITLE",)).get("GAME_TITLE", name)
    outputs = [f"{name}{ext}" for ext in (".nds", ".elf")]
    # (GAME_ICON is a path from where make runs: the app's folder, $(CURDIR))
    script = (f"make TARGET={shlex.quote(name)} GAME_ICON=$PWD/{ICON}" +
              (" SAKBOX_DEBUG=1" if args.debug else "") + devkitpro_image.make_defines(args.define, name, title) + " >&2 && "
              "tar -c " + " ".join(shlex.quote(o) for o in outputs))
    with tempfile.TemporaryDirectory() as scratch:
        icon = args.icon
        if not icon:
            icon = pathlib.Path(scratch, "icon.bmp")
            letters = args.icon_letters or app_images.icon_letters(title)
            font = app_images.EasyFont(pathlib.Path(args.font).read_text())
            app_images.write_bmp(app_images.icon(font, letters, ICON_SIZE, title), icon)
        files.append((str(icon), ICON))
        devkitpro_image.run(args.image, files, script, outputs, args.out_dir,
                            "The devkitPro build")


if __name__ == "__main__":
    main(sys.argv[1:])
