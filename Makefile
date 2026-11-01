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
LDFLAGS    = $(ARCH_FLAGS) -nostartfiles -Wl,--gc-sections -Wl,--build-id=none \
             -Wl,-Map=$(ARM_MAP)

ARM_OBJS = build/arm/custom.o build/arm/main.o build/arm/physics.o \
           build/arm/display_list.o build/arm/audio.o
ROM_SRCS = $(wildcard src/6502/*.asm) $(wildcard vendor/cdfjplus/*) $(LAYOUT)

.PHONY: all run clean
all: $(ROM)

# Shared memory map -> C header
$(LAYOUT_H): $(LAYOUT) tools/gen_layout.py
	@mkdir -p $(dir $@)
	$(PY) tools/gen_layout.py $(LAYOUT) $@

# ARM C and boot code
build/arm/%.o: src/arm/%.c $(LAYOUT_H) $(wildcard include/*.h) $(wildcard src/arm/*.h)
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
