"""Builds a 3DS homebrew app with devkitPro's Makefile inside the pinned
devkitARM image: the .3dsx (for the Homebrew Launcher), its .smdh and .elf,
and, given the CIA tools, a .cia (installed to the HOME Menu).

Usage: build_3ds.py IMAGE NAME OUT_DIR [CIA options] -- [SRC PATH]...
(each source file, then the path it goes at in the app's folder)

CIA options: --makerom FILE --bannertool FILE --rsf FILE --logo FILE
--unique-id ID --product-code CODE [--banner-image PNG] [--banner-sound WAV]
"""

import argparse
import math
import pathlib
import shlex
import struct
import sys
import tempfile
import zlib

import devkitpro_image

CIA_FOLDER = "_cia"  # in the app's folder: the CIA tools and their inputs


def default_banner_image(path):
    """A 256x128 PNG for the HOME Menu banner: a blue gradient."""
    width, height = 256, 128
    rows = b"".join(
        b"\x00" + bytes([20 + y // 4, 60 + y // 2, 140 + y // 2]) * width for y in range(height))

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
                     chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


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


def main(argv):
    if "--" not in argv:
        sys.exit(__doc__)
    split = argv.index("--")
    parser = argparse.ArgumentParser()
    parser.add_argument("image")
    parser.add_argument("name")
    parser.add_argument("out_dir")
    for option in ["makerom", "bannertool", "rsf", "logo", "unique-id", "product-code",
                   "banner-image", "banner-sound"]:
        parser.add_argument("--" + option)
    args = parser.parse_args(argv[:split])
    files = devkitpro_image.pairs(argv[split + 1:])
    if len(files) * 2 != len(argv) - split - 1:
        sys.exit(__doc__)
    name = args.name

    outputs = [f"{name}{ext}" for ext in (".3dsx", ".smdh", ".elf")]
    commands = [f"make TARGET={shlex.quote(name)} >&2"]
    with tempfile.TemporaryDirectory() as scratch:
        if args.makerom:
            image = args.banner_image
            sound = args.banner_sound
            if not image:
                image = pathlib.Path(scratch, "banner.png")
                default_banner_image(image)
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
