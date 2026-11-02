# Atari 2600 CDFJ+ platformer

A 32 KB CDFJ+ ROM for the Atari 2600, with the game logic on the ARM coprocessor
and the screen kernel on the 6507. So far: a hero who walks, jumps and shoots
across a scrolling tile-map stage with walking and flying enemies.

The hero has 3 hit points, shown as pips in the top-left. Touching an enemy
costs one and throws the hero back with a small hop; falling into a pit costs
one and returns to the start. After a hit the hero blinks and cannot be hurt for
1.5 seconds. At zero, everything resets with every enemy back.

Controls (Stella keyboard: arrows and Space): left/right walk, up jumps (hold
for a higher jump), fire shoots in the direction you face.

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

`make test` runs the level converter's tests. `make shot` runs Stella headless
for `FRAMES` frames (default 120) and saves `build/shot.png`.

Debug builds (touch `src/arm/game.c` first, since the flags don't trigger a rebuild):
`make START_STATE=1` starts on a screen (0 title, 1 stage select, 2 play), and
`make DEMO=1` adds a scripted player that walks title, select and into the stage.
The frame number `make shot` takes is not a count of game frames (it ran about
1.6 game frames per snapshot frame, varying from run to run), so sample several.

## Screens

The game runs title -> stage select -> play. Title and select are drawn into a
framebuffer (`src/arm/fb.c`): a 40-cell x 192-line bitmap (cells are the 4 px
playfield blocks) with a 3x5 font, so they have one-line vertical detail instead
of the tile map's 8 lines. The static art is drawn once and only the blinking
prompt, cursor and stage name are redrawn per frame. Cell columns 0-1 are hidden
by the HMOVE bar, so art sits in columns 2-39. Press fire on the title, move the
cursor on the 3x3 grid (the centre is the hero's portrait and is skipped), and
press fire on an unlocked stage. Only stage 1 (GROVE) is built; the other seven
show dithered icons.

All names, icons and art are original.

## Levels

A stage is a text file in `levels/` (currently `levels/stage1.txt`): 24 rows,
a multiple of 8 columns (at least 40), `#` solid, `.` empty, `S` the hero's
spawn (top-left corner). A tile is 4 px wide and 8 lines tall, so one screen is
40 x 24 tiles. `tools/level2c.py` turns it into `build/gen/level_data.c/.h` at
build time; `src/arm/level.h` reads it for both rendering and collision.

The playfield can only scroll in whole 4 px columns, so the camera snaps to a
column while the hero sprite moves pixel by pixel. Physics limits for level
design: the hero is 4 x 2 tiles, jumps about 5 rows high, and clears gaps of up
to about 7 columns. Falling off the bottom costs a hit point and respawns at `S`.
Keep row 0 empty: the health pips are drawn over it. `W` places a walker
(patrols its floor, turns at walls and edges) and `F` a flyer (patrols and
bobs); up to 16 per stage.

## Sprites

Every object is 16x16, drawn with the P0 + P1 pair. The kernel does `HMOVE` on
every line, with the motion streamed from the ARM, so the pair drifts from one
object's X to the next one's during the blank lines between them (8 px right or
7 px left per line), while the playfield keeps drawing. The cost is the black
8 px `HMOVE` bar on the left edge, and one color per object.

Bullets use missile 0 (4 px wide, 2 lines tall), positioned in VBLANK and
enabled per line from a stream. Up to two fly at once; with two, they take
turns being drawn. To fit the per-line `ENAM0` write, the visible loop runs
exactly 76 cycles without `WSYNC`, and the assembler checks its branch does not
cross a page.

Objects too close vertically for the drift take turns: each frame the one that
has waited longest is drawn, so they flicker rather than vanish. Objects partly
off the left or right edge are not drawn yet (they pop in).

Note for `make shot`: Stella needs a few dozen frames after power-on to lock
onto the frame, so snapshots before about frame 90 can show a rolling picture.

## Layout

| Path | What it is |
|---|---|
| `src/6502/main.asm` | ROM entry: driver, bank 0 code, C stack and entry words, ARM binary |
| `src/6502/kernel.asm` | Frame loop and visible kernel (76 cycles per line) |
| `src/6502/vectors.asm` | Reset vector and CDFJ+ C stack and entry words |
| `src/arm/main.c` | Frame entry, called once per frame in VBLANK |
| `src/arm/game.c` | State machine: title, stage select, play |
| `src/arm/screens.c` | Title and stage-select screens |
| `src/arm/fb.c` | Framebuffer drawing, font and playfield packer |
| `src/arm/physics.c` | Hero motion and tile collision, 8.8 fixed point |
| `src/arm/display_list.c` | Camera, and rebuilds the per-scanline tables every frame |
| `src/arm/level.h` | Tile map access shared by rendering and physics |
| `src/arm/input.h` | Joystick and fire bits passed from the 6507 |
| `src/arm/enemies.c` | Walker and flyer behaviour, hero contact |
| `src/arm/shots.c` | Hero bullets: fire, move, hit tiles and enemies |
| `src/arm/health.c` | Hit points and the invulnerability window |
| `levels/` | Stage text files |
| `tests/` | Level converter tests (`make test`) |
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
