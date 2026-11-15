"""The SVG converter: shapes, paths and curves, transforms, colours and
opacity, strokes as filled outlines, and drawings it refuses."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import svg_to_recipe  # noqa: E402


def recipe(body, view="0 0 10 10"):
    return svg_to_recipe.convert(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{view}">{body}</svg>')


def lines(text, kind):
    return [line for line in text.splitlines() if line.startswith(kind + " ")]


class ConvertTest(unittest.TestCase):
    def test_a_rectangle(self):
        text = recipe('<rect x="1" y="2" width="3" height="4" fill="#ff0000"/>')
        self.assertTrue(text.startswith("recipe 1\nsize 10 10\n"))
        self.assertEqual(lines(text, "colour"), ["colour 0 255 0 0 255"])
        self.assertEqual(lines(text, "fill"), ["fill 0 nonzero"])
        # In 1/16ths of a unit
        self.assertEqual(lines(text, "contour"), ["contour 16 32 64 32 64 96 16 96"])

    def test_colours_are_shared_and_opacity_kept(self):
        text = recipe('<rect width="1" height="1" fill="#0f0"/><circle cx="5" cy="5" r="2" fill="rgb(0,255,0)"/>'
                      '<rect width="2" height="2" fill="white" opacity="0.5"/>')
        self.assertEqual(lines(text, "colour"), ["colour 0 0 255 0 255", "colour 1 255 255 255 128"])
        self.assertEqual(len(lines(text, "fill")), 3)

    def test_every_css_colour_form(self):
        cases = {
            "chartreuse": "127 255 0 255", "RebeccaPurple": "102 51 153 255", "#f008": "255 0 0 136",
            "#00ff0080": "0 255 0 128", "rgba(0, 0, 255, 0.5)": "0 0 255 128",
            "rgb(100% 0% 0% / 50%)": "255 0 0 128", "hsl(120, 100%, 50%)": "0 255 0 255",
            "hsla(240deg 100% 50% / 0.25)": "0 0 255 64",
        }
        for written, expected in cases.items():
            with self.subTest(written):
                text = recipe(f'<rect width="1" height="1" fill="{written}"/>')
                self.assertEqual(lines(text, "colour"), [f"colour 0 {expected}"])

    def test_many_colours_warned_about(self):
        import contextlib
        import io
        import tempfile
        rects = "".join(f'<rect x="{i}" width="1" height="1" fill="rgb({i * 10},0,0)"/>' for i in range(20))
        with tempfile.TemporaryDirectory() as folder:
            svg = os.path.join(folder, "many.svg")
            with open(svg, "w") as f:
                f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 1">{rects}</svg>')
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                self.assertEqual(svg_to_recipe.main(["svg_to_recipe.py", svg, os.path.join(folder, "out")]), 0)
        self.assertIn("WARNING", err.getvalue())
        self.assertIn("20 colours: drawn as Palette16", err.getvalue())
        self.assertNotIn("Palette256", err.getvalue())

    def test_transforms_nest(self):
        text = recipe('<g transform="translate(2 3)"><g transform="scale(2)"><rect width="1" height="1"/></g></g>')
        self.assertEqual(lines(text, "contour"), ["contour 32 48 64 48 64 80 32 80"])

    def test_the_view_box_origin(self):
        text = recipe('<rect x="5" y="5" width="1" height="1"/>', view="5 5 10 10")
        self.assertEqual(lines(text, "contour"), ["contour 0 0 16 0 16 16 0 16"])

    def test_paths_lines_and_curves(self):
        text = recipe('<path d="M1 1 h4 v4 H1 z m6 0 l2 0 l0 2 z"/>')
        contours = lines(text, "contour")
        self.assertEqual(len(contours), 2)
        self.assertEqual(contours[0], "contour 16 16 80 16 80 80 16 80")
        self.assertEqual(contours[1], "contour 112 16 144 16 144 48")
        curve = lines(recipe('<path d="M0 0 C 0 10 10 10 10 0 Z"/>'), "contour")[0]
        points = curve.split()[1:]
        self.assertEqual(len(points) // 2, 1 + svg_to_recipe.CURVE_STEPS)  # the start, then the flattened curve
        self.assertEqual(points[-2:], ["160", "0"])

    def test_even_odd_holes(self):
        text = recipe('<path fill-rule="evenodd" d="M0 0 H10 V10 H0 Z M3 3 H7 V7 H3 Z"/>')
        self.assertEqual(lines(text, "fill"), ["fill 0 evenodd"])
        self.assertEqual(len(lines(text, "contour")), 2)

    def test_strokes_become_filled_outlines(self):
        text = recipe('<line x1="1" y1="5" x2="9" y2="5" stroke="blue" stroke-width="2"/>')
        self.assertEqual(lines(text, "fill"), ["fill 0 nonzero"])
        contours = lines(text, "contour")
        # One band and a round piece at each end
        self.assertEqual(len(contours), 3)
        self.assertEqual(contours[0], "contour 16 96 144 96 144 64 16 64")

    def test_no_fill_draws_nothing(self):
        self.assertEqual(lines(recipe('<rect width="1" height="1" fill="none"/>'), "fill"), [])

    def test_what_it_cannot_draw_is_refused(self):
        with self.assertRaises(svg_to_recipe.RecipeError):
            recipe('<path d="M0 0 A 5 5 0 0 1 10 10"/>')
        with self.assertRaises(svg_to_recipe.RecipeError):
            recipe('<text>hello</text>')
        with self.assertRaises(svg_to_recipe.RecipeError):
            recipe('<rect width="1" height="1" fill="notacolour"/>')
        with self.assertRaises(svg_to_recipe.RecipeError):
            svg_to_recipe.convert('<svg xmlns="http://www.w3.org/2000/svg"><rect width="1" height="1"/></svg>')


if __name__ == "__main__":
    unittest.main()
