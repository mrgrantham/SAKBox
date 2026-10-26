"""Builds makerom and bannertool, the tools that make 3DS CIA files, from
source inside devkitPro's image, so they run there (on whatever processor
Docker runs on).

Usage: build_cia_tools.py IMAGE OUT_DIR [SRC PATH]...
(each source file, then its path: makerom's under makerom/, bannertool's
under bannertool/)
"""

import sys

import devkitpro_image


def main(argv):
    if len(argv) < 2 or len(argv) % 2:
        sys.exit(__doc__)
    image, out_dir, *files = argv
    script = " && ".join([
        # makerom builds its bundled libraries first
        "(cd makerom && make deps && make) >&2",
        "mkdir out",
        "cp makerom/bin/makerom out/",
        # bannertool's own Makefile only knows x86 hosts; it's a few files
        "cd bannertool",
        "cc -O2 -w -c source/pc/stb_image.c source/pc/stb_vorbis.c >&2",
        "c++ -O2 -w -Isource -DVERSION_MAJOR=1 -DVERSION_MINOR=2 -DVERSION_MICRO=0 "
        "source/*.cpp source/pc/*.cpp source/3ds/*.cpp stb_image.o stb_vorbis.o -o ../out/bannertool >&2",
        "cd ..",
        "tar -C out -c makerom bannertool",
    ])
    devkitpro_image.run(image, devkitpro_image.pairs(files), script, ["makerom", "bannertool"],
                        out_dir, "Building the CIA tools")


if __name__ == "__main__":
    main(sys.argv[1:])
