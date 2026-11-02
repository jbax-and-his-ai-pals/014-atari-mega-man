#!/usr/bin/env python3
"""Convert a text sprite file (art/*.txt) into art_data.c / art_data.h.

Usage: art2c.py ART.txt OUT.c OUT.h

Input lines (';' comments and blank lines are ignored):
  palette X=HH ...      palette letters (A-Z) -> TIA color bytes (two hex digits)
  sprite NAME           starts a sprite
  frame NAME            starts a frame of the current sprite, followed by 16 rows
  row: C PPPPPPPPPPPPPPPP
       C is a palette letter, or '.' for a blank row (then all pixels must be '.')
       P is '.' or '#'

Bit order: pixel i (0 = leftmost) of a row is bit (15 - i) of its uint16 value.
"""
import os
import re
import sys

ROWS = 16
ROW_LEN = 18  # row color, space, 16 pixels
NAME_RE = re.compile(r"[a-z][a-z0-9_]*\Z")
PALETTE_RE = re.compile(r"([A-Z])=([0-9A-Fa-f]{2})\Z")
LETTER_RE = re.compile(r"[A-Z]\Z")
TOKEN_RE = re.compile(r"\S+")


class ArtError(Exception):
    pass


def parse_art(text, name="<art>"):
    """Return the frames in file order.

    Each frame is a dict with: sprite, frame, macro, line, col, rows, colors.
    rows holds uint16 values, colors holds TIA color bytes.
    """
    palette = {}                 # letter -> (color byte, line)
    sprites = {}                 # sprite name -> line
    frame_lines = {}             # frame name -> line, within the current sprite
    macros = {"ART_NUM_FRAMES": "the frame count"}  # macro -> what defines it
    frames = []
    sprite = None
    cur = None

    def err(n, col, msg):
        raise ArtError("%s:%d:%d: %s" % (name, n, col, msg))

    def close_frame(f):
        if f is not None and len(f["rows"]) != ROWS:
            err(f["line"], f["col"], "frame '%s_%s' has %d rows; expected %d"
                % (f["sprite"], f["frame"], len(f["rows"]), ROWS))

    for n, line in enumerate(text.splitlines(), 1):
        line = line.rstrip()
        tokens = [(m.start() + 1, m.group()) for m in TOKEN_RE.finditer(line)]
        if not tokens or tokens[0][1].startswith(";"):
            continue
        key_col, key = tokens[0]

        if key == "palette":
            close_frame(cur)
            cur = None
            if len(tokens) < 2:
                err(n, key_col, "palette needs at least one X=HH entry")
            for col, word in tokens[1:]:
                m = PALETTE_RE.match(word)
                if not m:
                    err(n, col, "bad palette entry %r (expected an uppercase letter, '=' and two hex digits)"
                        % word)
                letter = m.group(1)
                if letter in palette:
                    err(n, col, "palette letter %r already defined on line %d"
                        % (letter, palette[letter][1]))
                palette[letter] = (int(m.group(2), 16), n)

        elif key == "sprite":
            close_frame(cur)
            cur = None
            if len(tokens) != 2:
                err(n, key_col, "expected 'sprite NAME'")
            col, sname = tokens[1]
            if not NAME_RE.match(sname):
                err(n, col, "bad sprite name %r (use lowercase letters, digits and '_', starting with a letter)"
                    % sname)
            if sname in sprites:
                err(n, col, "duplicate sprite %r (first defined on line %d)" % (sname, sprites[sname]))
            sprites[sname] = n
            sprite = sname
            frame_lines = {}

        elif key == "frame":
            close_frame(cur)
            cur = None
            if len(tokens) != 2:
                err(n, key_col, "expected 'frame NAME'")
            if sprite is None:
                err(n, key_col, "frame before any sprite (add 'sprite NAME' first)")
            col, fname = tokens[1]
            if not NAME_RE.match(fname):
                err(n, col, "bad frame name %r (use lowercase letters, digits and '_', starting with a letter)"
                    % fname)
            if fname in frame_lines:
                err(n, col, "duplicate frame %r in sprite %r (first defined on line %d)"
                    % (fname, sprite, frame_lines[fname]))
            frame_lines[fname] = n
            macro = ("ART_%s_%s" % (sprite, fname)).upper()
            if macro in macros:
                err(n, col, "frame '%s_%s' would define %s, which is already used by %s"
                    % (sprite, fname, macro, macros[macro]))
            macros[macro] = "frame '%s_%s'" % (sprite, fname)
            cur = {"sprite": sprite, "frame": fname, "macro": macro, "line": n,
                   "col": key_col, "rows": [], "colors": []}
            frames.append(cur)

        else:
            # Anything else must be a row inside a frame.
            if cur is None:
                err(n, key_col, "unexpected %r (expected 'palette', 'sprite' or 'frame')" % key)
            if len(cur["rows"]) == ROWS:
                err(cur["line"], cur["col"], "frame '%s_%s' has more than %d rows"
                    % (cur["sprite"], cur["frame"], ROWS))
            color_ch = line[:1]
            if color_ch == ".":
                color = 0
            elif color_ch in palette:
                color = palette[color_ch][0]
            elif LETTER_RE.match(color_ch):
                err(n, 1, "palette letter %r is not defined" % color_ch)
            else:
                err(n, 1, "row color %r must be '.' or a palette letter" % color_ch)
            if line[1:2] != " ":
                err(n, 2, "expected one space after the row color")
            pix = line[2:]
            if len(pix) != ROWS:
                err(n, min(len(line), ROW_LEN) + 1, "row has %d pixels; expected %d"
                    % (len(pix), ROWS))
            for i, ch in enumerate(pix):
                if ch not in ".#":
                    err(n, i + 3, "invalid pixel %r (use '.' or '#')" % ch)
            if color_ch == "." and "#" in pix:
                err(n, pix.index("#") + 3, "'#' in a row colored '.'; a blank row must be all '.'")
            value = 0
            for i, ch in enumerate(pix):
                if ch == "#":
                    value |= 1 << (15 - i)
            cur["rows"].append(value)
            cur["colors"].append(color)

    close_frame(cur)
    if not frames:
        raise ArtError("%s: no frames defined" % name)
    return frames


def _wrap(values, fmt):
    parts = [fmt % v for v in values]
    lines = [", ".join(parts[i:i + 8]) for i in range(0, len(parts), 8)]
    return (",\n" + " " * 10).join(lines)


def render(src_name, frames):
    n = len(frames)
    hdr = [
        "/* Generated from %s by tools/art2c.py. Do not edit. */" % src_name,
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
        "#define ART_NUM_FRAMES %du" % n,
    ]
    for i, f in enumerate(frames):
        hdr.append("#define %s %du" % (f["macro"], i))
    hdr += ["", "extern const art_frame_t art_frames[ART_NUM_FRAMES];", "", "#endif"]

    out = [
        "/* Generated from %s by tools/art2c.py. Do not edit. */" % src_name,
        '#include "art_data.h"',
        "",
        "const art_frame_t art_frames[ART_NUM_FRAMES] = {",
    ]
    for f in frames:
        out.append("    /* %s_%s */" % (f["sprite"], f["frame"]))
        out.append("    {")
        out.append("        { " + _wrap(f["rows"], "0x%04X") + " },")
        out.append("        { " + _wrap(f["colors"], "0x%02X") + " }")
        out.append("    },")
    out.append("};")
    return "\n".join(out) + "\n", "\n".join(hdr) + "\n"


def main(argv):
    if len(argv) != 4:
        sys.stderr.write("usage: art2c.py ART.txt OUT.c OUT.h\n")
        return 2
    src, outc, outh = argv[1:]
    try:
        with open(src, "r", encoding="ascii", errors="strict", newline=None) as f:
            text = f.read()
        frames = parse_art(text, src)
    except (ArtError, OSError, UnicodeDecodeError) as e:
        sys.stderr.write("art2c: error: %s\n" % e)
        return 1
    csrc, hdr = render(src, frames)
    try:
        for path, content in ((outc, csrc), (outh, hdr)):
            d = os.path.dirname(path)
            if d:
                os.makedirs(d, exist_ok=True)
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(content)
    except OSError as e:
        sys.stderr.write("art2c: error: %s\n" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
