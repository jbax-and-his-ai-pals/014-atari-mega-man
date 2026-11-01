"""Turn include/layout.inc into a C header.

usage: gen_layout.py include/layout.inc build/gen/layout.h
"""
import pathlib
import re
import sys

ASSIGN = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(\$[0-9A-Fa-f]+|\d+)\s*(;.*)?$")


def main() -> int:
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])
    out = [
        "/* Generated from include/layout.inc by tools/gen_layout.py. Do not edit. */",
        "#ifndef LAYOUT_H",
        "#define LAYOUT_H",
    ]
    for number, line in enumerate(src.read_text().splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith(";"):
            continue
        m = ASSIGN.match(line)
        if not m:
            if re.match(r"^\s*[A-Za-z_]\w*\s*=", line):
                print(f"{src}:{number}: unsupported assignment: {line.strip()}", file=sys.stderr)
                return 1
            continue
        name, value = m.group(1), m.group(2)
        if value.startswith("$"):
            value = "0x" + value[1:]
        out.append(f"#define {name} {value}u")
    out.append("#endif")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("\n".join(out) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
