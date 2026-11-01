"""Sanity-check the finished 32K CDFJ+ ROM.

usage: check_rom.py build/game.bin build/arm/armcode.bin
"""
import pathlib
import struct
import sys


def main() -> int:
    rom = pathlib.Path(sys.argv[1]).read_bytes()
    arm = pathlib.Path(sys.argv[2]).read_bytes()
    errors = []

    def u32(off: int) -> int:
        return struct.unpack_from("<I", rom, off)[0]

    if len(rom) != 0x8000:
        errors.append(f"ROM is {len(rom)} bytes, expected 32768")
    if rom[0x184:0x18C] != b"PLUSCDFJ" or u32(0x18C) != 1:
        errors.append("CDFJ+ signature or version 1 missing at $0184")
    if u32(0x17F4) != 0x40001FDC:
        errors.append(f"C stack word at $17F4 is {u32(0x17F4):#x}, expected 0x40001fdc")
    base = u32(0x17F8)
    if (base & 0xFFFFFFFE) != 0x2800 or not (base & 1):
        errors.append(f"C entry word at $17F8 is {base:#x}, expected 0x2801")
    reset = struct.unpack_from("<H", rom, 0x17FC)[0]
    if not 0x1000 <= reset <= 0x1FF3:
        errors.append(f"reset vector {reset:#x} is outside bank 0 code")
    if rom[0x2800:0x2800 + len(arm)] != arm:
        errors.append("ARM binary is not at $2800")

    if errors:
        print("FAIL:")
        for e in errors:
            print("  " + e)
        return 1
    print(f"ROM check: pass. Reset at {reset:#06x}, C entry at $2800, ARM {len(arm)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
