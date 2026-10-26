"""Builds a Nintendo DS homebrew app with devkitPro's Makefile inside the
pinned devkitARM image: the .nds (to run on a DS, from a flash card, or in an
emulator) and its .elf.

Usage: build_nds.py IMAGE NAME OUT_DIR -- [SRC PATH]...
(each source file, then the path it goes at in the app's folder)
"""

import shlex
import sys

import devkitpro_image


def main(argv):
    if "--" not in argv:
        sys.exit(__doc__)
    split = argv.index("--")
    if split != 3:
        sys.exit(__doc__)
    image, name, out_dir = argv[:3]
    files = devkitpro_image.pairs(argv[split + 1:])
    if len(files) * 2 != len(argv) - split - 1:
        sys.exit(__doc__)
    outputs = [f"{name}{ext}" for ext in (".nds", ".elf")]
    script = (f"make TARGET={shlex.quote(name)} >&2 && "
              "tar -c " + " ".join(shlex.quote(o) for o in outputs))
    devkitpro_image.run(image, files, script, outputs, out_dir, "The devkitPro build")


if __name__ == "__main__":
    main(sys.argv[1:])
