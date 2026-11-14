"""Tests for devkitpro_image's make_defines: the app's name and title, and a
target's defines, reach the compiler whole after make's command line and a
recipe's shell have each taken their quoting off."""

import shlex
import unittest

import devkitpro_image


def compiler_flags(make_argument):
    """What the compiler gets: make's command line parsed (as the build's
    shell does), then the variable's value parsed again (as a recipe's
    shell does)."""
    (assignment,) = shlex.split(make_argument)
    variable, value = assignment.split("=", 1)
    assert variable == "SAKBOX_DEFINES"
    return shlex.split(value)


class MakeDefinesTest(unittest.TestCase):
    def test_name_and_title_come_first(self):
        flags = compiler_flags(devkitpro_image.make_defines([], "cyclone-ds-ragdoll",
                                                            "Cyclone ragdoll"))
        self.assertEqual(flags, ['-DSCRATCHPADD_APP_NAME="cyclone-ds-ragdoll"',
                                 '-DSCRATCHPADD_APP_TITLE="Cyclone ragdoll"'])

    def test_a_title_with_quotes_and_backslashes_stays_one_c_string(self):
        flags = compiler_flags(devkitpro_image.make_defines([], "app", 'Say "hi" \\ bye'))
        self.assertEqual(flags[1], '-DSCRATCHPADD_APP_TITLE="Say \\"hi\\" \\\\ bye"')

    def test_a_targets_defines_follow(self):
        flags = compiler_flags(devkitpro_image.make_defines(
            ["RAGDOLL_SWEEP_AT_START", "LEVEL=2"], "app", "App"))
        self.assertEqual(flags[2:], ["-DRAGDOLL_SWEEP_AT_START", "-DLEVEL=2"])


if __name__ == "__main__":
    unittest.main()
