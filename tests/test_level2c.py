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
        import re
        cols = int(re.search(r"LEVEL_COLS\s+(\d+)u", read(h)).group(1))
        stride = cols // 8 + 4
        self.assertEqual(len(data), 24 * stride)
        self.assertEqual(data[:stride], [0] * stride)  # row 0 empty


def place(text, items):
    """Put enemy chars into a make_level(comment=False) text: items = (row, col, ch)."""
    lines = text.splitlines()
    for r, c, ch in items:
        lines[r] = lines[r][:c] + ch + lines[r][c + 1:]
    return "\n".join(lines) + "\n"


class TestEnemies(Base):
    def base(self, **kw):
        return make_level(comment=False, spawn=(0, 0), **kw)

    def enemy_entries(self, csrc):
        import re
        body = csrc.split("level_enemies[", 1)[1].split("};", 1)[0]
        return [tuple(int(v) for v in m) for m in
                re.findall(r"\{\s*(-?\d+),\s*(-?\d+),\s*(\d+)\s*\}", body)]

    def test_parse_order_and_coords(self):
        # out of reading order on purpose; output must be row-major
        text = place(self.base(cols=64), [(10, 30, "F"), (5, 40, "W"), (5, 12, "F"),
                                          (20, 3, "W")])
        csrc, hdr = self.ok(text)
        self.assertEqual(self.enemy_entries(csrc),
                         [(48, 40, 1), (160, 40, 0), (120, 80, 1), (12, 160, 0)])
        self.assertIn("#define ENEMY_WALKER  0u", hdr)
        self.assertIn("#define ENEMY_FLYER   1u", hdr)
        self.assertIn("#define LEVEL_NUM_ENEMIES 4u", hdr)
        self.assertIn("typedef struct { int16_t x; int16_t y; uint8_t type; } level_enemy_t;", hdr)
        self.assertIn("extern const level_enemy_t level_enemies[4];", hdr)
        self.assertIn("const level_enemy_t level_enemies[4] = {", csrc)

    def test_tiles_are_empty(self):
        csrc, _ = self.ok(place(self.base(), [(2, 8, "W"), (2, 9, "F")]))
        data = bytes_from_c(csrc.split("level_enemies", 1)[0])
        stride = 9
        self.assertEqual(data[2 * stride + 1], 0)
        self.assertEqual(sum(data), 0)

    def test_zero_enemies_dummy(self):
        csrc, hdr = self.ok(self.base())
        self.assertIn("#define LEVEL_NUM_ENEMIES 0u", hdr)
        self.assertIn("extern const level_enemy_t level_enemies[1];", hdr)
        self.assertEqual(self.enemy_entries(csrc), [(0, 0, 0)])

    def test_sixteen_ok(self):
        items = [(r, 10 + c, "W") for r in (2, 4) for c in range(8)]
        csrc, hdr = self.ok(place(self.base(), items))
        self.assertIn("#define LEVEL_NUM_ENEMIES 16u", hdr)
        self.assertEqual(len(self.enemy_entries(csrc)), 16)

    def test_seventeen_fails(self):
        items = [(r, 10 + c, "F") for r in (2, 4) for c in range(8)] + [(6, 5, "W")]
        # the 17th in reading order is at line 6 (1-based), col 6
        self.fail(place(self.base(), items), "lvl.txt:7:6", "too many enemies")


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


class TestHazardsConveyors(Base):
    def base(self, **kw):
        return make_level(comment=False, spawn=(0, 0), **kw)

    def entries(self, csrc, arr):
        import re
        body = csrc.split(arr + "[", 1)[1].split("};", 1)[0]
        return [tuple(int(v) for v in m.split(",")) for m in
                re.findall(r"\{([^{}]*)\}", body)]

    def solid(self, csrc, r, c, cols=40):
        stride = cols // 8 + 4
        data = bytes_from_c(csrc.split("level_enemies", 1)[0])
        return (data[r * stride + (c >> 3)] >> (c & 7)) & 1

    def test_conveyors_solid_and_runs(self):
        # row 4: '<' then '>' adjacent -> two runs; row 10: '>' run, gap, '<' run, '>' alone
        items = [(4, 0, "<"), (4, 1, ">"),
                 (10, 5, ">"), (10, 6, ">"), (10, 7, ">"),
                 (10, 12, "<"), (10, 13, "<"),
                 (10, 20, ">")]
        csrc, hdr = self.ok(place(self.base(), items))
        for r, c in ((4, 0), (4, 1), (10, 5), (10, 13), (10, 20)):
            self.assertEqual(self.solid(csrc, r, c), 1)
        self.assertEqual(self.solid(csrc, 10, 8), 0)
        self.assertEqual(self.entries(csrc, "level_conveyors"),
                         [(0, 32, 4, -1), (4, 32, 4, 1),
                          (20, 80, 12, 1), (48, 80, 8, -1), (80, 80, 4, 1)])
        self.assertIn("#define LEVEL_NUM_CONVEYORS 5u", hdr)
        self.assertIn("extern const level_conveyor_t level_conveyors[5];", hdr)
        self.assertIn("typedef struct { int16_t x; int16_t y; uint16_t w; int8_t dir; } "
                      "level_conveyor_t;", hdr)

    def test_hazards_empty_and_coords(self):
        # out of reading order on purpose; output must be row-major
        items = [(9, 2, "O"), (3, 10, "O"), (7, 30, "P"), (5, 20, "U")]
        csrc, hdr = self.ok(place(self.base(cols=64), items))
        self.assertEqual(sum(bytes_from_c(csrc.split("level_enemies", 1)[0])), 0)
        self.assertEqual(self.entries(csrc, "level_hazards"),
                         [(40, 24, 0), (80, 40, 1), (120, 56, 2), (8, 72, 0)])
        self.assertIn("#define HAZ_SAW_H    0u", hdr)
        self.assertIn("#define HAZ_SAW_V    1u", hdr)
        self.assertIn("#define HAZ_CRUSHER  2u", hdr)
        self.assertIn("#define LEVEL_NUM_HAZARDS 4u", hdr)
        self.assertIn("extern const level_hazard_t level_hazards[4];", hdr)
        self.assertIn("const level_hazard_t level_hazards[4] = {", csrc)

    def test_twenty_four_hazards_ok(self):
        items = [(r, c, "O") for r in range(1, 13) for c in (10, 20)]
        csrc, hdr = self.ok(place(self.base(), items))
        self.assertIn("#define LEVEL_NUM_HAZARDS 24u", hdr)
        self.assertEqual(len(self.entries(csrc, "level_hazards")), 24)

    def test_twenty_fifth_hazard_fails(self):
        items = [(r, c, "O") for r in range(1, 13) for c in (10, 20)] + [(13, 5, "P")]
        # the 25th in reading order is at line 14 (1-based), col 6
        self.fail(place(self.base(), items), "lvl.txt:14:6", "too many hazards")

    def test_twenty_four_conveyor_runs_ok(self):
        items = [(r, c, ">") for r in range(1, 13) for c in (2, 4)]
        csrc, hdr = self.ok(place(self.base(), items))
        self.assertIn("#define LEVEL_NUM_CONVEYORS 24u", hdr)
        self.assertEqual(len(self.entries(csrc, "level_conveyors")), 24)

    def test_twenty_fifth_conveyor_run_fails(self):
        items = [(r, c, ">") for r in range(1, 13) for c in (2, 4)] + [(13, 2, "<")]
        self.fail(place(self.base(), items), "lvl.txt:14:3", "too many conveyor runs")

    def test_zero_hazards_and_conveyors_dummy(self):
        csrc, hdr = self.ok(self.base())
        self.assertIn("#define LEVEL_NUM_HAZARDS 0u", hdr)
        self.assertIn("extern const level_hazard_t level_hazards[1];", hdr)
        self.assertEqual(self.entries(csrc, "level_hazards"), [(0, 0, 0)])
        self.assertIn("#define LEVEL_NUM_CONVEYORS 0u", hdr)
        self.assertIn("extern const level_conveyor_t level_conveyors[1];", hdr)
        self.assertEqual(self.entries(csrc, "level_conveyors"), [(0, 0, 0, 0)])
        self.assertIn("#define LEVEL_NUM_ENEMIES 0u", hdr)

    def test_bad_char_lists_new_chars(self):
        lines = make_level(comment=False).splitlines()
        lines[2] = lines[2][:5] + "x" + lines[2][6:]
        err = self.fail("\n".join(lines) + "\n", "lvl.txt:3:6")
        self.assertIn("'O', 'U', 'P', '>', '<'", err)


if __name__ == "__main__":
    unittest.main()
