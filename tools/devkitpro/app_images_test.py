"""app_images: the default icons' letters, size and colours, and the
files written (a PNG, and a BMP as a DS icon must be). Its argument is
stb_easy_font.h's path."""

import pathlib
import struct
import sys
import tempfile
import unittest

import app_images

FONT_PATH = None  # (from the command line)


class AppImagesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.font = app_images.EasyFont(pathlib.Path(FONT_PATH).read_text())

    def test_icon_letters_are_the_titles_first_two(self):
        self.assertEqual(app_images.icon_letters("Blender Demo"), "Bl")
        self.assertEqual(app_images.icon_letters("3DS benchmark"), "3D")
        self.assertEqual(app_images.icon_letters("A b"), "Ab")

    def test_icon_is_the_size_asked_with_two_colours(self):
        for size in (48, 32):
            pixels = app_images.icon(self.font, "Bl", size, "Blender Demo")
            self.assertEqual((len(pixels), len(pixels[0])), (size, size))
            colours = {pixel for row in pixels for pixel in row}
            self.assertEqual(len(colours), 2)
            # the letters are drawn (more of the bright colour than the border alone)
            border = pixels[0][0]
            self.assertGreater(sum(row.count(border) for row in pixels), 4 * (size - 1) + 40)

    def test_same_letters_different_titles_differ_in_colour(self):
        one = app_images.icon(self.font, "Sc", 48, "ScratchPadd demo")
        other = app_images.icon(self.font, "Sc", 48, "Scene 3DS benchmark")
        self.assertNotEqual(one[0][0], other[0][0])

    def test_bmp_is_an_8_bit_16_colour_one(self):
        pixels = app_images.icon(self.font, "Cy", 32, "Cyclone ragdoll")
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder, "icon.bmp")
            app_images.write_bmp(pixels, path)
            data = path.read_bytes()
        self.assertEqual(data[:2], b"BM")
        size, _, _, offset = struct.unpack("<IHHI", data[2:14])
        self.assertEqual(size, len(data))
        header, width, height, _, bits, _, _, _, _, colours, _ = struct.unpack(
            "<IiiHHIIiiII", data[14:54])
        self.assertEqual((header, width, height, bits, colours), (40, 32, 32, 8, 16))
        self.assertEqual(offset, 14 + 40 + 16 * 4)
        self.assertEqual(len(data), offset + 32 * 32)
        # palette entry 0 (transparent on a DS) isn't used; the top-left
        # pixel, the last row's first, is the border's colour
        indices = data[offset:]
        self.assertNotIn(0, indices)
        r, g, b = pixels[0][0]
        index = indices[31 * 32]
        self.assertEqual(data[54 + 4 * index:54 + 4 * index + 3], bytes((b, g, r)))

    def test_bmp_refuses_too_many_colours(self):
        pixels = [[(i, i, i) for i in range(17)]]
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):
                app_images.write_bmp(pixels, pathlib.Path(folder, "icon.bmp"))

    def test_png_has_the_banners_size(self):
        pixels = app_images.banner(self.font, "Models from Blender", "A description", "SAKBox")
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder, "banner.png")
            app_images.write_png(pixels, path)
            data = path.read_bytes()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", data[16:24]), (256, 128))


if __name__ == "__main__":
    FONT_PATH = sys.argv.pop(1)
    unittest.main()
