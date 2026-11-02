import os
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, "tools", "art2c.py")
REAL = os.path.join(ROOT, "art", "sprites.txt")

BLANK = ". " + "." * 16


def read(path):
    with open(path) as f:
        return f.read()


def frame(name, rows=()):
    """A 'frame NAME' line plus 16 rows, padded with blank rows."""
    return ["frame %s" % name] + list(rows) + [BLANK] * (16 - len(rows))


def art(*sections):
    lines = []
    for s in sections:
        lines += s
    return "\n".join(lines) + "\n"


def line_of(text, s, nth=1):
    hits = [i for i, l in enumerate(text.splitlines(), 1) if l == s]
    return hits[nth - 1]


def c_ints(csrc):
    body = csrc.split("art_frames[ART_NUM_FRAMES] = {", 1)[1].rsplit("};", 1)[0]
    body = re.sub(r"/\*.*?\*/", "", body)
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", body)]


def frame_values(csrc, index):
    """Return (rows, colors) of frame index from the generated C file."""
    v = c_ints(csrc)[index * 32:(index + 1) * 32]
    return v[:16], v[16:]


class Base(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.dir = self._td.name
        self.src = os.path.join(self.dir, "art.txt")
        self.c = os.path.join(self.dir, "out", "art_data.c")
        self.h = os.path.join(self.dir, "out", "art_data.h")

    def loc(self, n, col):
        return "%s:%d:%d" % (self.src, n, col)

    def run_tool(self, text):
        with open(self.src, "w") as f:
            f.write(text)
        return subprocess.run([sys.executable, TOOL, self.src, self.c, self.h],
                              capture_output=True, text=True)

    def ok(self, text):
        p = self.run_tool(text)
        self.assertEqual(p.returncode, 0, p.stderr)
        return read(self.c), read(self.h)

    def fail(self, text, *needles):
        p = self.run_tool(text)
        self.assertEqual(p.returncode, 1, p.stderr)
        self.assertTrue(p.stderr.startswith("art2c: error: "), p.stderr)
        for n in needles:
            self.assertIn(n, p.stderr)
        return p.stderr


class TestBitOrder(Base):
    def test_bit_order(self):
        text = art(["palette A=46"], ["sprite hero"],
                   frame("run_a", ["A ....####........",
                                   "A #...............",
                                   "A ...............#",
                                   "A ################"]))
        csrc, _ = self.ok(text)
        rows, colors = frame_values(csrc, 0)
        self.assertEqual(rows[:4], [0x0F00, 0x8000, 0x0001, 0xFFFF])
        self.assertEqual(rows[4:], [0] * 12)
        self.assertEqual(colors[:4], [0x46] * 4)

    def test_blank_rows_are_zero(self):
        csrc, _ = self.ok(art(["palette A=46"], ["sprite hero"], frame("idle")))
        rows, colors = frame_values(csrc, 0)
        self.assertEqual(rows, [0] * 16)
        self.assertEqual(colors, [0] * 16)


class TestColors(Base):
    def test_palette_colors(self):
        text = art(["palette A=46 B=0E", "palette C=0a"], ["sprite hero"],
                   frame("run_a", ["A ####............",
                                   "B ....#...........",
                                   ". ................",
                                   "A ................",
                                   "C ...............#"]))
        csrc, _ = self.ok(text)
        rows, colors = frame_values(csrc, 0)
        self.assertEqual(colors[:5], [0x46, 0x0E, 0, 0x46, 0x0A])
        # all-'.' row with a palette color: no pixels, but the color is kept
        self.assertEqual(rows[2], 0)
        self.assertEqual(rows[3], 0)
        self.assertEqual(rows[4], 0x0001)
        self.assertEqual(rows[0], 0xF000)


class TestFrames(Base):
    def setUp(self):
        super().setUp()
        self.text = art(["palette A=01"], ["sprite hero"],
                        frame("run_a", ["A #..............."]),
                        frame("run_b"),
                        ["sprite enemy"],
                        frame("idle"))

    def test_indices_and_defines(self):
        _, hdr = self.ok(self.text)
        self.assertIn("#define ART_NUM_FRAMES 3u", hdr)
        self.assertIn("#define ART_HERO_RUN_A 0u", hdr)
        self.assertIn("#define ART_HERO_RUN_B 1u", hdr)
        self.assertIn("#define ART_ENEMY_IDLE 2u", hdr)
        self.assertIn("extern const art_frame_t art_frames[ART_NUM_FRAMES];", hdr)

    def test_c_file_order(self):
        csrc, _ = self.ok(self.text)
        self.assertIn('#include "art_data.h"', csrc)
        self.assertIn("const art_frame_t art_frames[ART_NUM_FRAMES] = {", csrc)
        i = [csrc.index("/* %s */" % s) for s in ("hero_run_a", "hero_run_b", "enemy_idle")]
        self.assertEqual(i, sorted(i))
        rows, _ = frame_values(csrc, 0)
        self.assertEqual(rows[0], 0x8000)
        self.assertEqual(frame_values(csrc, 2)[0], [0] * 16)

    def test_header_exact(self):
        _, hdr = self.ok(art(["palette A=46"], ["sprite hero"], frame("run_a")))
        expected = "\n".join([
            "/* Generated from %s by tools/art2c.py. Do not edit. */" % self.src,
            "#ifndef ART_DATA_H",
            "#define ART_DATA_H",
            "",
            "#include <stdint.h>",
            "",
            "typedef struct {",
            "    uint16_t rows[16];      /* bit 15 = leftmost pixel; high byte -> GRP0, low byte -> GRP1 */",
            "    uint8_t colors[16];     /* TIA color byte for each row; 0 for a blank row */",
            "} art_frame_t;",
            "",
            "#define ART_NUM_FRAMES 1u",
            "#define ART_HERO_RUN_A 0u",
            "",
            "extern const art_frame_t art_frames[ART_NUM_FRAMES];",
            "",
            "#endif",
        ]) + "\n"
        self.assertEqual(hdr, expected)


class TestErrors(Base):
    def test_unknown_palette_letter(self):
        row = "Z #..............."
        text = art(["palette A=46"], ["sprite hero"], frame("run_a", [row]))
        self.fail(text, self.loc(line_of(text, row), 1), "'Z' is not defined")

    def test_row_width_short(self):
        row = "A ....#.........."  # 15 pixels
        text = art(["palette A=46"], ["sprite hero"], frame("run_a", [row]))
        self.fail(text, self.loc(line_of(text, row), 18), "row has 15 pixels")

    def test_row_width_long(self):
        row = "A " + "." * 17  # 17 pixels
        text = art(["palette A=46"], ["sprite hero"], frame("run_a", [row]))
        self.fail(text, self.loc(line_of(text, row), 19), "row has 17 pixels")

    def test_row_count_short(self):
        text = art(["palette A=46"], ["sprite hero"], ["frame run_a"] + [BLANK] * 15)
        self.fail(text, self.loc(line_of(text, "frame run_a"), 1), "has 15 rows; expected 16")

    def test_row_count_long(self):
        text = art(["palette A=46"], ["sprite hero"], ["frame run_a"] + [BLANK] * 17)
        self.fail(text, self.loc(line_of(text, "frame run_a"), 1), "more than 16 rows")

    def test_hash_in_blank_row(self):
        row = ". ....#..........."
        text = art(["palette A=46"], ["sprite hero"], frame("run_a", [row]))
        self.fail(text, self.loc(line_of(text, row), 7), "'#' in a row colored '.'")

    def test_duplicate_frame(self):
        text = art(["palette A=46"], ["sprite hero"], frame("run_a"), frame("run_a"))
        self.fail(text, self.loc(line_of(text, "frame run_a", 2), 1), "duplicate frame 'run_a'")

    def test_duplicate_sprite(self):
        text = art(["palette A=46"], ["sprite hero"], frame("run_a"),
                   ["sprite hero"], frame("run_b"))
        self.fail(text, self.loc(line_of(text, "sprite hero", 2), 1), "duplicate sprite 'hero'")

    def test_frame_before_sprite(self):
        text = art(["palette A=46"], frame("run_a"))
        self.fail(text, self.loc(2, 1), "before any sprite")

    def test_bad_sprite_name(self):
        text = art(["palette A=46"], ["sprite Hero"], frame("run_a"))
        self.fail(text, self.loc(2, 8), "bad sprite name 'Hero'")

    def test_bad_frame_name(self):
        text = art(["palette A=46"], ["sprite hero"], frame("1run"))
        self.fail(text, self.loc(3, 7), "bad frame name '1run'")

    def test_redefined_palette_letter(self):
        text = art(["palette A=46"], ["palette B=01 A=10"])
        self.fail(text, self.loc(2, 14), "palette letter 'A' already defined on line 1")

    def test_bad_palette_entry(self):
        text = art(["palette A=4"], ["sprite hero"], frame("run_a"))
        self.fail(text, self.loc(1, 9), "bad palette entry 'A=4'")

    def test_empty_file(self):
        self.fail("; nothing here\n\n", "no frames defined")
        self.assertFalse(os.path.exists(self.c))

    def test_row_outside_frame(self):
        text = art(["palette A=46"], ["sprite hero"], ["A ................"])
        self.fail(text, self.loc(3, 1), "unexpected 'A'")

    def test_unknown_directive(self):
        text = art(["palette A=46"], ["bogus"])
        self.fail(text, self.loc(2, 1), "unexpected 'bogus'")

    def test_macro_clash(self):
        text = art(["palette A=46"], ["sprite a"], frame("b_c"),
                   ["sprite a_b"], frame("c"))
        self.fail(text, self.loc(line_of(text, "frame c"), 7), "would define ART_A_B_C")

    def test_macro_clash_with_count(self):
        text = art(["palette A=46"], ["sprite num"], frame("frames"))
        self.fail(text, self.loc(line_of(text, "frame frames"), 7), "ART_NUM_FRAMES")

    def test_missing_input(self):
        p = subprocess.run([sys.executable, TOOL, os.path.join(self.dir, "nope.txt"),
                            "a.c", "a.h"], capture_output=True, text=True)
        self.assertEqual(p.returncode, 1)
        self.assertTrue(p.stderr.startswith("art2c: error: "), p.stderr)

    def test_usage(self):
        p = subprocess.run([sys.executable, TOOL, "only_one_arg.txt"],
                           capture_output=True, text=True)
        self.assertEqual(p.returncode, 2)
        self.assertIn("usage", p.stderr)


class TestRealSprites(Base):
    def test_real_sprites(self):
        if not os.path.exists(REAL):
            self.skipTest("art/sprites.txt not present")
        p = subprocess.run([sys.executable, TOOL, REAL, self.c, self.h],
                           capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("#define ART_NUM_FRAMES", read(self.h))


if __name__ == "__main__":
    unittest.main()
