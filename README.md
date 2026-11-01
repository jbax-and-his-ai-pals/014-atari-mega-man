# Atari 2600 CDFJ+ platformer (Phase 1 scaffold)

A 32 KB CDFJ+ ROM for the Atari 2600, with the game logic on the ARM coprocessor
and the screen kernel on the 6507. This is the Phase 1 scaffold: a bouncing hero,
a sweeping ledge, and a floor, all rendered through the datastream path.

## Build and run

Run the toolchain setup once, then build:

```bash
py -3 tools/setup_toolchain.py
source ./env.sh
make
make run
```

`make` produces `build/game.bin`. `make run` starts Stella with developer flags:
the stats overlay (frame length and rate), the Thumb trap on fatal errors, and
detection info. The overlay should read `262 / 60.0Hz => NTSC`.

## Layout

| Path | What it is |
|---|---|
| `src/6502/main.asm` | ROM entry: driver, bank 0 code, C stack and entry words, ARM binary |
| `src/6502/kernel.asm` | Frame loop and visible kernel (76 cycles per line) |
| `src/6502/vectors.asm` | Reset vector and CDFJ+ C stack and entry words |
| `src/arm/main.c` | Frame entry, called once per frame in VBLANK |
| `src/arm/physics.c` | Hero motion, 8.8 fixed point |
| `src/arm/display_list.c` | Rebuilds the per-scanline tables every frame |
| `src/arm/audio.c` | Stub |
| `include/layout.inc` | Shared memory map, read by DASM and by `tools/gen_layout.py` |
| `include/cdfj.h` | Datastream pointer and increment helpers |
| `vendor/cdfjplus/` | CDFJ+ 48-pin driver and includes (see its README) |
| `tools/` | Toolchain setup, layout generator, ARM guard, ROM check |

## Memory map

- `$0000-$07FF`: CDFJ+ driver (2 KB, from the template).
- `$0800-$17FF`: bank 0, with the kernel at 6507 `$1000`.
- `$17F4-$17FB`: C stack and C entry words (never executed by the 6507).
- `$1800-$27FF`: bank 1, free for 6507 code.
- `$2800-$7FEF`: ARM code and data (banks 2-6). The C code starts at `$2800`, not
  the template's `$6800`, which leaves about 22 KB for ARM code.
- ARM RAM: display data at `$40000800`, with 12 datastreams. Pointers at
  `$40000098`, increments at `$40000124`. The magic init word is at `$40001FE0`.

## Things that matter for later work

- **Boot RAM is not zeroed.** Stella randomizes RAM, and the boot code clears
  `.bss` only on its first call, based on a counter. Nothing guarantees that
  counter starts at zero. So any C state must be set explicitly. The init gate in
  `main.c` uses a magic word for this reason.
- **ARM work must fit in VBLANK.** The display table is rebuilt every frame and is
  kept cheap for that reason. If the stats overlay shows the frame growing past
  262 lines, the ARM work has overrun VBLANK. Trim the work, or move some of it to
  overscan.
- **Division and 64-bit helpers are ARM-mode in libgcc.** The ARM guard
  (`tools/check_arm.py`) fails the build if any ARM-mode helper is linked. The
  display code avoids them; the 64-bit mask is only shifted and tested.
- **The driver is the 48-pin variant.** It is correct for Stella at 32 KB, but
  real hardware above 32 KB needs the 64-pin driver.
- **Audio is a stub.** `SETMODE` is `$00` (fast fetch on, no audio mode), matching
  the template's start-up. Music mode (`$F0`) starts the driver's audio interrupt,
  which we don't use yet.

## Verified in Stella

- Frame length is 262 lines at 60.0 Hz NTSC.
- Playfield, sky, floor, the ledge gap sweep, and the hero's position and
  animation all render correctly.

## Not yet verified

- The hero is calibrated by cycle timing, not by pixel measurement. Its position
  and seams have only been checked by eye.
- The flicker budget for two sprites on one line has not been measured.
- Real hardware has not been tested.
