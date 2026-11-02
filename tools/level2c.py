#!/usr/bin/env python3
"""Convert a text level (levels/*.txt) into level_data.c / level_data.h.

Usage: level2c.py LEVEL.txt OUT.c OUT.h
Bit order: column c of row r is bit (c & 7) of byte r*STRIDE + (c >> 3); set = solid.
"""
import os
import sys

ROWS = 24
MIN_COLS = 40
MAX_ENEMIES = 16
ENEMY_CHARS = {"W": 0, "F": 1}  # char -> type id
HAZARD_CHARS = {"O": 0, "U": 1, "P": 2}  # char -> HAZ_* type id (saw H, saw V, crusher)
CONVEYOR_DIRS = {">": 1, "<": -1}  # char -> dir (solid tiles)
MAX_HAZARDS = 24
MAX_CONVEYORS = 24


class LevelError(Exception):
    pass


def parse_level(text, name="<level>"):
    """Return (cols, rows_of_bools, spawn_col, spawn_row, enemies, hazards, conveyors).

    enemies and hazards are lists of (col, row, type) in reading order.
    conveyors is a list of (row, start_col, tile_count, char) runs in reading order.
    """
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
    enemies = []
    hazards = []
    conveyors = []  # runs: [row, first col, tile count, char]
    for r, (n, line) in enumerate(lines):
        row = []
        run = None  # conveyor run that the previous column belongs to
        for c, ch in enumerate(line):
            prev, run = run, None
            if ch in CONVEYOR_DIRS:
                row.append(True)
                if prev is not None and prev[3] == ch:
                    run = prev
                    run[2] += 1
                else:
                    if len(conveyors) >= MAX_CONVEYORS:
                        raise LevelError("%s:%d:%d: too many conveyor runs (maximum %d)"
                                         % (name, n, c + 1, MAX_CONVEYORS))
                    run = [r, c, 1, ch]
                    conveyors.append(run)
            elif ch == "#":
                row.append(True)
            elif ch == ".":
                row.append(False)
            elif ch == "S":
                row.append(False)
                spawns.append((r, c, n))
            elif ch in ENEMY_CHARS:
                row.append(False)
                if len(enemies) >= MAX_ENEMIES:
                    raise LevelError("%s:%d:%d: too many enemies (maximum %d)"
                                     % (name, n, c + 1, MAX_ENEMIES))
                enemies.append((c, r, ENEMY_CHARS[ch]))
            elif ch in HAZARD_CHARS:
                row.append(False)
                if len(hazards) >= MAX_HAZARDS:
                    raise LevelError("%s:%d:%d: too many hazards (maximum %d)"
                                     % (name, n, c + 1, MAX_HAZARDS))
                hazards.append((c, r, HAZARD_CHARS[ch]))
            else:
                raise LevelError("%s:%d:%d: invalid character %r (use '#', '.', 'S', 'W', 'F', "
                                 "'O', 'U', 'P', '>', '<')" % (name, n, c + 1, ch))
        grid.append(row)
    if not spawns:
        raise LevelError("%s: no spawn 'S' found (exactly one required)" % name)
    if len(spawns) > 1:
        where = ", ".join("%d:%d" % (n, c + 1) for _, c, n in spawns)
        raise LevelError("%s: multiple spawns 'S' (at line:col %s); exactly one required"
                         % (name, where))
    return cols, grid, spawns[0][1], spawns[0][0], enemies, hazards, conveyors


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


def render(src_name, cols, grid, sc, sr, enemies=(), hazards=(), conveyors=()):
    ne = len(enemies)
    nd = max(ne, 1)
    nh = len(hazards)
    nhd = max(nh, 1)
    nc = len(conveyors)
    ncd = max(nc, 1)
    stride, data = pack(cols, grid)
    hdr = (
        "/* Generated from %s by tools/level2c.py. Do not edit. */\n"
        "#ifndef LEVEL_DATA_H\n#define LEVEL_DATA_H\n#include <stdint.h>\n"
        "#define LEVEL_COLS    %du\n"
        "#define LEVEL_ROWS    %du\n"
        "#define LEVEL_STRIDE  %du      /* bytes per row: bitmap + 4 zero bytes of padding */\n"
        "#define LEVEL_SPAWN_X %d\n"
        "#define LEVEL_SPAWN_Y %d\n"
        "#define ENEMY_WALKER  0u\n"
        "#define ENEMY_FLYER   1u\n"
        "#define LEVEL_NUM_ENEMIES %du\n"
        "typedef struct { int16_t x; int16_t y; uint8_t type; } level_enemy_t;"
        "   /* top-left, pixels: x = 4*col, y = 8*row */\n"
        "extern const uint8_t level_bits[LEVEL_ROWS * LEVEL_STRIDE];\n"
        "extern const level_enemy_t level_enemies[%d];\n"
        "#define HAZ_SAW_H    0u\n"
        "#define HAZ_SAW_V    1u\n"
        "#define HAZ_CRUSHER  2u\n"
        "#define LEVEL_NUM_HAZARDS %du\n"
        "typedef struct { int16_t x; int16_t y; uint8_t type; } level_hazard_t;"
        "   /* top-left, pixels: x = 4*col, y = 8*row */\n"
        "extern const level_hazard_t level_hazards[%d];\n"
        "#define LEVEL_NUM_CONVEYORS %du\n"
        "typedef struct { int16_t x; int16_t y; uint16_t w; int8_t dir; } level_conveyor_t;"
        "   /* a run of conveyor tiles: top-left px, width px (4 per tile), dir +1 right / -1 left */\n"
        "extern const level_conveyor_t level_conveyors[%d];\n"
        "#endif\n" % (src_name, cols, ROWS, stride, 4 * sc, 8 * sr, ne, nd,
                      nh, nhd, nc, ncd))
    out = ["/* Generated from %s by tools/level2c.py. Do not edit. */" % src_name,
           '#include "level_data.h"', "",
           "const uint8_t level_bits[LEVEL_ROWS * LEVEL_STRIDE] = {"]
    for r in range(ROWS):
        chunk = data[r * stride:(r + 1) * stride]
        out.append("    /* row %2d */ %s," % (r, ", ".join("0x%02X" % v for v in chunk)))
    out.append("};")
    out.append("")
    out.append("const level_enemy_t level_enemies[%d] = {" % nd)
    if enemies:
        for c, r, t in enemies:
            out.append("    { %d, %d, %d }," % (4 * c, 8 * r, t))
    else:
        out.append("    { 0, 0, 0 },")
    out.append("};")
    out.append("")
    out.append("const level_hazard_t level_hazards[%d] = {" % nhd)
    if hazards:
        for c, r, t in hazards:
            out.append("    { %d, %d, %d }," % (4 * c, 8 * r, t))
    else:
        out.append("    { 0, 0, 0 },")
    out.append("};")
    out.append("")
    out.append("const level_conveyor_t level_conveyors[%d] = {" % ncd)
    if conveyors:
        for r, c, tiles, ch in conveyors:
            out.append("    { %d, %d, %d, %d }," % (4 * c, 8 * r, 4 * tiles, CONVEYOR_DIRS[ch]))
    else:
        out.append("    { 0, 0, 0, 0 },")
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
        cols, grid, sc, sr, enemies, hazards, conveyors = parse_level(
            text, src.replace("\\", "/"))
    except (LevelError, OSError, UnicodeDecodeError) as e:
        sys.stderr.write("level2c: error: %s\n" % e)
        return 1
    csrc, hdr = render(src.replace("\\", "/"), cols, grid, sc, sr, enemies,
                       hazards, conveyors)
    for path, content in ((outc, csrc), (outh, hdr)):
        d = os.path.dirname(path)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(path, "w", newline="\n") as f:
            f.write(content)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
