# Development environment

Everything lives inside this folder. Nothing is installed system-wide.

## Toolchain (`toolchain/`, not committed)

| Tool | Version | Path |
|---|---|---|
| DASM (6502 assembler) | 2.20.17 (the binary reports 2.20.16) | `toolchain/dasm/dasm.exe` |
| ARM GCC (xPack arm-none-eabi) | 15.2.1-1.1 | `toolchain/arm/bin/` |
| GNU Make (xPack windows-build-tools) | 4.4.1 | `toolchain/make/bin/` |
| Stella | 7.0c (portable) | `toolchain/stella/Stella.exe` |

Install or reinstall (downloads about 340 MB, checks pinned SHA-256 hashes):

```
py -3 tools/setup_toolchain.py          # add --force to re-unpack
```

Put the tools on PATH for one shell session:

```
. .\env.ps1        # PowerShell
source ./env.sh    # Git Bash
```

Python: use `py -3` or `C:\Python314\python`. On this machine, `python3`
resolves to the Microsoft Store stub. There is no host C compiler, so build
helpers are written in Python.

## Running Stella for development

```
toolchain\stella\Stella.exe -basedir toolchain\stella-config -dev.settings 1 -dev.thumb.trapfatal 1 -dev.detectedinfo 1 <rom.bin>
```

- `-basedir` keeps Stella's settings and save states inside the project.
- `-dev.thumb.trapfatal 1` stops in the debugger on ARM faults instead of only logging them.
- `-dev.thumb.incycles 1` adds ARM run time to the 6507 clock, so ARM overruns
  show up as a rolling or jittering frame. The timing is approximate and it
  distorts audio, so turn it on only when profiling.
- `-dev.thumb.chiptype 0|1` (LPC2103 / LPC2104) and `-dev.thumb.mammode` change
  the emulated flash timing.
- Gopher2600 is a second emulator with more detailed ARM timing. It is not
  installed but is worth adding later to check the ARM time budget.

## Verified so far

- The community CDFJ+ template (see `vendor/cdfjplus/README.md`) builds with
  this toolchain into a 32 KB ROM. The ROM has the `PLUSCDFJ` v1 signature and
  correct C stack/entry words, and Stella 7.0 launches it in developer mode.
  Confirmed on screen in Stella 7.0c with the Phase 1 scaffold (see Lessons
  from Phase 1 below).
- `-mcpu=arm7tdmi -mthumb` selects the `thumb/nofp` multilib.

## Lessons from Phase 1

Checked while bringing up the Phase 1 scaffold in Stella 7.0c. The short version
is in `../README.md` under "Things that matter for later work".

- **RAM is random at power-on.** In developer mode Stella randomizes RAM. The boot
  code in `src/arm/boot/custom.S` clears `.bss` and copies `.data` only when the
  counter word at `0x40000024` goes from 0 to 1. That is not guaranteed, so C code
  must not rely on zeroed `.bss` or initialized `.data`. `main.c` runs its
  one-time init when the word at `0x40001FE0` does not hold its magic value, then
  writes the value. That word sits just above the C stack top, `0x40001FDC`.
- **Datastream increments must be written.** The pointers were correct, but the
  screen was noise while the increments were random. `display_list.c` now sets
  the pointer and increment for every stream each frame.
- **ARM time counts against VBLANK.** Stella keeps the TIA running while the ARM
  executes CALLFN. One over-budget ARM frame showed as 456 scanlines, and Stella
  switched to PAL detection. The stats overlay must read `262 / 60.0Hz => NTSC`.
- **SETMODE is `$00` at boot.** That means fast fetch on and no audio mode, the same
  as the template. `$F0` (music mode) enables the driver's audio interrupt, which
  is not used yet.
- **Datastream register layout** matches the template's `defines_cdfjplus.c`:
  pointers at `0x40000098` (offset << 16), increments at `0x40000124`
  ((whole << 8) | frac), and display data at `0x40000800`.
- **Verified on screen in Stella 7.0c:** a 262-line NTSC frame, an asymmetric
  playfield, and a 16-px hero made of two sprites with no seam.
- **Headless screenshots:** `make shot` (or `python tools/snap.py`) runs Stella
  for N frames through a debugger autoexec script and saves `build/shot.png`
  (320x228, the TIA image without the stats overlay). It cannot report the
  scanline count or inject input, so frame length still needs `make run`.

## Constraints found (read before writing ARM code)

### 1. libgcc division is ARM-mode, and Stella only runs Thumb

Even in the `thumb/nofp` multilib, `__aeabi_idiv`, `__aeabi_uidiv`,
`__aeabi_idivmod`, `__aeabi_uidivmod` and `__aeabi_ldivmod` are 32-bit ARM code,
reached through `*_from_thumb` veneers. Stella's Thumbulator only executes
Thumb, so any C `/` or `%` on a non-constant value will break. Checked and
fine (Thumb): `__aeabi_lmul` (64-bit multiply) and `__aeabi_llsl`/`llsr`/`lasr`
(64-bit shifts).

**Plan:**
- Keep `-lgcc` so the Thumb helpers above still link.
- Define our own Thumb `__aeabi_uidiv`, `__aeabi_uidivmod`, `__aeabi_idiv` and
  `__aeabi_idivmod`. With every symbol of libgcc's division members defined,
  the linker never pulls those members in. If it did, the build would fail on
  duplicate definitions.
- Never use 64-bit division.
- Supply `memcpy` and `memset`, since GCC can emit calls to them on its own.
- Prefer shifts, lookup tables and repeated subtraction in hot code anyway.

**Post-link guard (add to the build):** fail if the ELF contains any ARM-mode
mapping symbol or interworking veneer. That catches helpers nobody tested.

```
arm-none-eabi-objdump -t game.elf | awk '$NF=="$a" || $NF ~ /_from_(thumb|arm)$/ {bad=1; print} END {exit bad}'
```

Tested: the template's ARM code passes this check, and a test file using
`/` and `%` fails it.

### 2. The CPU is an ARM7TDMI, not a Cortex-M

Harmony and Melody carts use NXP LPC210x/LPC213x chips (ARM7TDMI-S, up to
70 MHz). Compile with `-mcpu=arm7tdmi -march=armv4t -mthumb`. Thumb-2, `blx`,
`cbz` and hardware divide are not available. There is no FPU, so use fixed
point (for example 8.8 or 12.4 sub-pixels).

GCC 15 defaults to C23 and treats implicit declarations as errors. Pass
`-std=gnu17` if older homebrew C needs it.

### 3. CDFJ+ ROM layout, as Stella reads it

Taken from Stella's `CartCDF.cxx`:

| File offset | Contents |
|---|---|
| `0x0000-0x07FF` | 2 KB driver. Must contain `PLUSCDFJ` + version 1. Copied into cart RAM at power-on. |
| `0x0800 + n*0x1000` | 6507 bank n (n = 0-6, seen at `$1000-$1FFF`). The console starts in bank 0. There are always 7 banks, at any ROM size. |
| `0x17F4` | C stack pointer (bank 0, `$1FF4`) |
| `0x17F8` | C entry address, Thumb bit set (bank 0, `$1FF8`) |
| after the last 6507 bank | ARM C code and data. Any 6507 bank the game doesn't use can hold C code instead. |

These are file offsets only: `$1FF4-$1FFA` are also bank-switch hotspots, so
the 6507 never executes from them. Bank 0's reset vector is at `0x17FC`.

ARM memory map: ROM at `0x00000000`. Cart RAM at `0x40000000`, where the first
2 KB holds the driver copy. Display data starts at `0x40000800`, and C
variables and the stack come after it.

- **6507 hotspots:** `$1FF0` DSWRITE, `$1FF1` DSPTR, `$1FF2` SETMODE,
  `$1FF3` CALLFN (`#$FE` = run ARM with IRQ audio, `#$FF` = run ARM without),
  `$1FF4-$1FFA` select banks 0-6.
- **Fast fetch:** while SETMODE's low nibble is 0, `LDA #n` (and, if enabled in
  the driver bytes, `LDX #n` / `LDY #n`) reads data stream `n - offset`.
  Streams 0-31 are data, `$20` is the comm stream, `$21`/`$22` are the
  fast-jump streams for `JMP $0000` / `JMP $0001`, and `$23` is AMPLITUDE
  (audio).
- **Stream pointers** are 16.16 fixed point into display RAM. Increments are
  8.8, so an increment of `0x0080` advances one byte every two reads (useful for
  two-line-resolution data).
- **Stream registers** live in driver RAM: pointers at `0x40000098`,
  increments at `0x40000124`, waveforms at `0x400001B0`.

### 4. Driver variant

The vendored driver is the 48-pin version (32 KB ROM / 8 KB RAM on real
hardware). Stella accepts it at any ROM size and always provides 32 KB of RAM.
Using it with a 256 KB ROM is an emulator-only choice: real hardware would
need the 64-pin driver and a board built around an LPC213x chip.
