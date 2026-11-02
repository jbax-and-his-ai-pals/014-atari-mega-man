import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, "tools", "level2c.py")


def read(path):
    with open(path) as f:
        return f.read()


def make_level(cols=40, rows=24, spawn=(0, 0), solid=(), comment=True):
    grid = [["."] * cols for _ in range(rows)]
    for r, c in solid:
        grid[r][c] = "#"
    grid[spawn[0]][spawn[1]] = "S"
    lines = ["".join(r) for r in grid]
    if comment:
        lines.insert(3, "; a comment line")
    return "\n".join(lines) + "\n"


class Base(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.dir = self._td.name

    def run_tool(self, text):
        src = os.path.join(self.dir, "lvl.txt")
        with open(src, "w") as f:
            f.write(text)
        c = os.path.join(self.dir, "out", "level_data.c")
        h = os.path.join(self.dir, "out", "level_data.h")
        p = subprocess.run([sys.executable, TOOL, src, c, h],
                           capture_output=True, text=True)
        return p, c, h

    def ok(self, text):
        p, c, h = self.run_tool(text)
        self.assertEqual(p.returncode, 0, p.stderr)
        return read(c), read(h)

    def fail(self, text, *needles):
        p, c, h = self.run_tool(text)
        self.assertNotEqual(p.returncode, 0)
        for n in needles:
            self.assertIn(n, p.stderr)
        return p.stderr


def bytes_from_c(csrc):
    body = csrc.split("= {", 1)[1].rsplit("}", 1)[0]
    import re
    body = re.sub(r"/\*.*?\*/", "", body)
    return [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", body)]


class TestGood(Base):
    def test_bit_order_and_padding(self):
        solid = [(0, 0), (0, 7), (1, 8), (2, 9), (5, 39), (23, 3)]
        csrc, hdr = self.ok(make_level(solid=solid, spawn=(10, 5)))
        data = bytes_from_c(csrc)
        stride = 40 // 8 + 4
        self.assertEqual(stride, 9)
        self.assertEqual(len(data), 24 * stride)
        exp = [0] * (24 * stride)
        exp[0 * stride + 0] = 0x81
        exp[1 * stride + 1] = 0x01
        exp[2 * stride + 1] = 0x02
        exp[5 * stride + 4] = 0x80
        exp[23 * stride + 0] = 0x08
        self.assertEqual(data, exp)
        for r in range(24):
            self.assertEqual(data[r * stride + 5:(r + 1) * stride], [0] * 4)

    def test_header(self):
        csrc, hdr = self.ok(make_level(cols=48, spawn=(3, 6)))
        self.assertIn("#define LEVEL_COLS    48u", hdr)
        self.assertIn("#define LEVEL_ROWS    24u", hdr)
        self.assertIn("#define LEVEL_STRIDE  10u", hdr)
        self.assertIn("#define LEVEL_SPAWN_X 24\n", hdr)
        self.assertIn("#define LEVEL_SPAWN_Y 24\n", hdr)
        self.assertIn("extern const uint8_t level_bits[LEVEL_ROWS * LEVEL_STRIDE];", hdr)
        self.assertIn('#include "level_data.h"', csrc)
        self.assertIn("const uint8_t level_bits[LEVEL_ROWS * LEVEL_STRIDE] = {", csrc)

    def test_spawn_not_solid(self):
        csrc, _ = self.ok(make_level(spawn=(0, 0)))
        self.assertEqual(bytes_from_c(csrc)[0], 0)

    def test_real_stage(self):
        stage = os.path.join(ROOT, "levels", "stage1.txt")
        c = os.path.join(self.dir, "a.c")
        h = os.path.join(self.dir, "a.h")
        p = subprocess.run([sys.executable, TOOL, stage, c, h],
                           capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        data = bytes_from_c(read(c))
        self.assertEqual(len(data), 24 * 36)
        self.assertEqual(data[:36], [0] * 36)  # row 0 empty


class TestErrors(Base):
    def test_row_count(self):
        t = make_level().splitlines()
        self.fail("\n".join(t[:-1]) + "\n", "expected 24")
        self.fail("\n".join(t + ["." * 40]) + "\n", "expected 24")

    def test_ragged(self):
        lines = make_level(comment=False).splitlines()
        lines[7] += "."
        self.fail("\n".join(lines) + "\n", ":8:")

    def test_width_not_multiple(self):
        self.fail(make_level(cols=44), "multiple of 8")

    def test_width_too_small(self):
        self.fail(make_level(cols=32), "minimum")

    def test_missing_spawn(self):
        lines = make_level(comment=False).replace("S", ".")
        self.fail(lines, "no spawn")

    def test_duplicate_spawn(self):
        lines = make_level(comment=False).splitlines()
        lines[4] = "S" + lines[4][1:]
        self.fail("\n".join(lines) + "\n", "multiple spawns")

    def test_bad_char(self):
        lines = make_level(comment=False).splitlines()
        lines[2] = lines[2][:5] + "x" + lines[2][6:]
        err = self.fail("\n".join(lines) + "\n", "lvl.txt:3:6")
        self.assertIn("invalid character", err)

    def test_missing_file(self):
        p = subprocess.run([sys.executable, TOOL, os.path.join(self.dir, "nope"),
                            "a.c", "a.h"], capture_output=True, text=True)
        self.assertNotEqual(p.returncode, 0)


if __name__ == "__main__":
    unittest.main()
