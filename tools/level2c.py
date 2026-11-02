#!/usr/bin/env python3
"""Convert a text level (levels/*.txt) into level_data.c / level_data.h.

Usage: level2c.py LEVEL.txt OUT.c OUT.h
Bit order: column c of row r is bit (c & 7) of byte r*STRIDE + (c >> 3); set = solid.
"""
import os
import sys

ROWS = 24
MIN_COLS = 40


class LevelError(Exception):
    pass


def parse_level(text, name="<level>"):
    """Return (cols, rows_of_bools, spawn_col, spawn_row)."""
    lines = []  # (lineno, text)
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip("\r\n")
        if line.startswith(";"):
            continue
        lines.append((n, line))
    if len(lines) != ROWS:
        raise LevelError("%s: expected %d tile rows, found %d" % (name, ROWS, len(lines)))
    cols = len(lines[0][1])
    for n, line in lines:
        if len(line) != cols:
            raise LevelError("%s:%d: row length %d differs from first row length %d"
                             % (name, n, len(line), cols))
    if cols % 8 != 0:
        raise LevelError("%s: width %d is not a multiple of 8" % (name, cols))
    if cols < MIN_COLS:
        raise LevelError("%s: width %d is less than the minimum %d" % (name, cols, MIN_COLS))
    grid = []
    spawns = []
    for r, (n, line) in enumerate(lines):
        row = []
        for c, ch in enumerate(line):
            if ch == "#":
                row.append(True)
            elif ch == ".":
                row.append(False)
            elif ch == "S":
                row.append(False)
                spawns.append((r, c, n))
            else:
                raise LevelError("%s:%d:%d: invalid character %r (use '#', '.', 'S')"
                                 % (name, n, c + 1, ch))
        grid.append(row)
    if not spawns:
        raise LevelError("%s: no spawn 'S' found (exactly one required)" % name)
    if len(spawns) > 1:
        where = ", ".join("%d:%d" % (n, c + 1) for _, c, n in spawns)
        raise LevelError("%s: multiple spawns 'S' (at line:col %s); exactly one required"
                         % (name, where))
    return cols, grid, spawns[0][1], spawns[0][0]


def pack(cols, grid):
    stride = cols // 8 + 4
    data = bytearray()
    for row in grid:
        b = bytearray(stride)
        for c, solid in enumerate(row):
            if solid:
                b[c >> 3] |= 1 << (c & 7)
        data += b
    return stride, bytes(data)


def render(src_name, cols, grid, sc, sr):
    stride, data = pack(cols, grid)
    hdr = (
        "/* Generated from %s by tools/level2c.py. Do not edit. */\n"
        "#ifndef LEVEL_DATA_H\n#define LEVEL_DATA_H\n#include <stdint.h>\n"
        "#define LEVEL_COLS    %du\n"
        "#define LEVEL_ROWS    %du\n"
        "#define LEVEL_STRIDE  %du      /* bytes per row: bitmap + 4 zero bytes of padding */\n"
        "#define LEVEL_SPAWN_X %d\n"
        "#define LEVEL_SPAWN_Y %d\n"
        "extern const uint8_t level_bits[LEVEL_ROWS * LEVEL_STRIDE];\n"
        "#endif\n" % (src_name, cols, ROWS, stride, 4 * sc, 8 * sr))
    out = ["/* Generated from %s by tools/level2c.py. Do not edit. */" % src_name,
           '#include "level_data.h"', "",
           "const uint8_t level_bits[LEVEL_ROWS * LEVEL_STRIDE] = {"]
    for r in range(ROWS):
        chunk = data[r * stride:(r + 1) * stride]
        out.append("    /* row %2d */ %s," % (r, ", ".join("0x%02X" % v for v in chunk)))
    out.append("};")
    return "\n".join(out) + "\n", hdr


def main(argv):
    if len(argv) != 4:
        sys.stderr.write("usage: level2c.py LEVEL.txt OUT.c OUT.h\n")
        return 2
    src, outc, outh = argv[1:]
    try:
        with open(src, "r", encoding="ascii", errors="strict", newline=None) as f:
            text = f.read()
        cols, grid, sc, sr = parse_level(text, src.replace("\\", "/"))
    except (LevelError, OSError, UnicodeDecodeError) as e:
        sys.stderr.write("level2c: error: %s\n" % e)
        return 1
    csrc, hdr = render(src.replace("\\", "/"), cols, grid, sc, sr)
    for path, content in ((outc, csrc), (outh, hdr)):
        d = os.path.dirname(path)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(path, "w", newline="\n") as f:
            f.write(content)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
