"""Post-link guard for the ARM build.

Fails if the ELF contains ARM-mode code (mapping symbol $a) or ARM<->Thumb
interworking veneers, because Stella's Thumbulator only runs Thumb. Also checks
that the ROM payload fits in the ARM space ($2800-$7FEF).

usage: check_arm.py armcode.elf armcode.bin
"""
import pathlib
import subprocess
import sys

CAPACITY = 0x7FF0 - 0x2800  # must match custom.boot.lds


def main() -> int:
    elf, binary = sys.argv[1], pathlib.Path(sys.argv[2])
    listing = subprocess.run(
        ["arm-none-eabi-objdump", "-t", elf],
        capture_output=True, text=True, check=True,
    ).stdout

    bad = []
    for line in listing.splitlines():
        fields = line.split()
        if not fields:
            continue
        name = fields[-1]
        if name == "$a" or name.endswith("_from_thumb") or name.endswith("_from_arm"):
            bad.append(line)
    if bad:
        print("FAIL: ARM-mode code or interworking veneers in the ELF:")
        print("\n".join(bad))
        return 1

    size = binary.stat().st_size
    if size > CAPACITY:
        print(f"FAIL: armcode.bin is {size} bytes, capacity {CAPACITY}")
        return 1
    print(f"ARM guard: pass. armcode.bin {size} of {CAPACITY} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
