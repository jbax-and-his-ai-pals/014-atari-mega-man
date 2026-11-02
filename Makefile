# CDFJ+ build: ARM C (arm-none-eabi-gcc) -> binary -> DASM 6507 kernel + ROM.
# Needs arm-none-eabi-gcc, dasm and Python 3 on PATH (run: source ./env.sh).
#
#   make          build build/game.bin
#   make run      build, then start Stella with developer settings
#   make clean    remove build/

PY      ?= python
DASM    ?= dasm
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size
STELLA  ?= toolchain/stella/Stella.exe

ROM       = build/game.bin
LISTING   = build/game.lst
SYMBOLS   = build/game.sym
ARM_ELF   = build/arm/armcode.elf
ARM_MAP   = build/arm/armcode.map
ARM_BIN   = build/arm/armcode.bin
LAYOUT    = include/layout.inc
LAYOUT_H  = build/gen/layout.h

ARCH_FLAGS = -mcpu=arm7tdmi -march=armv4t -mthumb
CFLAGS     = $(ARCH_FLAGS) -std=gnu17 -Os -Wall -ffunction-sections -fdata-sections \
             -Iinclude -Isrc/arm -Ibuild/gen
# Debug: start on a screen (GAME_TITLE=0, GAME_SELECT=1, GAME_PLAY=2); touch src/arm/game.c to apply.
CFLAGS    += $(if $(START_STATE),-DSTART_STATE=$(START_STATE))
# Debug: a scripted player walks the title and select screens (see game.c).
CFLAGS    += $(if $(DEMO),-DDEMO_INPUT)
LDFLAGS    = $(ARCH_FLAGS) -nostartfiles -Wl,--gc-sections -Wl,--build-id=none \
             -Wl,-Map=$(ARM_MAP)

ARM_OBJS = build/arm/custom.o build/arm/main.o build/arm/physics.o \
           build/arm/display_list.o build/arm/audio.o \
           build/arm/level_data.o build/arm/enemies.o build/arm/shots.o \
           build/arm/health.o build/arm/fb.o build/arm/screens.o build/arm/game.o \
           build/arm/art_data.o build/arm/intro.o
ROM_SRCS = $(wildcard src/6502/*.asm) $(wildcard vendor/cdfjplus/*) $(LAYOUT)

.PHONY: all run clean test
all: $(ROM)

# Shared memory map -> C header
$(LAYOUT_H): $(LAYOUT) tools/gen_layout.py
	@mkdir -p $(dir $@)
	$(PY) tools/gen_layout.py $(LAYOUT) $@

# Level text -> C data (one generator run produces both files)
LEVEL_SRC = levels/stage1.txt
build/gen/level_data.c build/gen/level_data.h &: $(LEVEL_SRC) tools/level2c.py
	@mkdir -p $(dir $@)
	$(PY) tools/level2c.py $(LEVEL_SRC) build/gen/level_data.c build/gen/level_data.h

build/arm/level_data.o: build/gen/level_data.c build/gen/level_data.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Sprite art text -> C data
ART_SRC = art/sprites.txt
build/gen/art_data.c build/gen/art_data.h &: $(ART_SRC) tools/art2c.py
	@mkdir -p $(dir $@)
	$(PY) tools/art2c.py $(ART_SRC) build/gen/art_data.c build/gen/art_data.h

build/arm/art_data.o: build/gen/art_data.c build/gen/art_data.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test:
	$(PY) -m unittest discover -s tests -v

# ARM C and boot code
build/arm/%.o: src/arm/%.c $(LAYOUT_H) build/gen/level_data.h build/gen/art_data.h $(wildcard include/*.h) $(wildcard src/arm/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/arm/custom.o: src/arm/boot/custom.S $(LAYOUT_H)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ARM_ELF): $(ARM_OBJS) src/arm/boot/custom.boot.lds
	$(CC) $(LDFLAGS) -T src/arm/boot/custom.boot.lds -o $@ $(ARM_OBJS)
	$(SIZE) $@

$(ARM_BIN): $(ARM_ELF)
	$(OBJCOPY) -O binary -S $< $@
	$(PY) tools/check_arm.py $< $@

# 6507 kernel + driver + ARM binary -> ROM
$(ROM): $(ROM_SRCS) $(ARM_BIN)
	@mkdir -p build
	$(DASM) src/6502/main.asm -f3 -v3 -Iinclude -Isrc/6502 -Ivendor/cdfjplus -Ibuild/gen \
		-o$@ -l$(LISTING) -s$(SYMBOLS)
	$(PY) tools/check_rom.py $@ $(ARM_BIN)

run: $(ROM)
	$(STELLA) -basedir toolchain/stella-config -dev.settings 1 -dev.thumb.trapfatal 1 -dev.detectedinfo 1 -dev.stats 1 $(ROM) &

clean:
	rm -rf build

# Headless screenshot: build, then run Stella unattended for N frames and save a PNG.
shot: $(ROM)
	$(PY) tools/snap.py $(ROM) --frames $(or $(FRAMES),120) --out build/shot.png
