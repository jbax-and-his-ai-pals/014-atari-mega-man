;@ main.asm - CDFJ+ 32K ROM entry point. Build with make (see Makefile).
;@
;@ ROM map (file offset = 6507/ARM address for the first 32 KB):
;@   $0000-$07FF  CDFJ+ driver (vendor/cdfjplus, 48-pin variant)
;@   $0800-$17FF  bank 0: boot, frame loop, visible kernel (6507 $1000-$1FF3)
;@   $17F4-$17FB  C stack and C entry words (6507 $1FF4-$1FFB; never executed)
;@   $17FC-$17FF  6507 reset and BRK vectors
;@   $1800-$27FF  bank 1: free 6507 code space
;@   $2800-$7FEF  ARM C code and data (C_START = $2800, banks 2-6)
;@   $7FFF        padding
	PROCESSOR 6502

FF_LDX    = $A2             ; do not change: driver option, fast fetch for LDX
FF_LDY    = $A0             ; do not change: driver option, fast fetch for LDY
FF_OFFSET = 200             ; LDA #(FF_OFFSET + n) reads datastream n

	INCLUDE "layout.inc"
	INCLUDE "cdfjplus.h"
	INCLUDE "vcs.h"
	INCLUDE "tia.inc"

	SEG CODE

;@ CDFJ+ driver, 2048 bytes. The three option bytes sit between the pieces.
	ORG $0000
	INCBIN "vendor/cdfjplus/cdfjplus48_p1.bin"
	.byte FF_LDX
	INCBIN "vendor/cdfjplus/cdfjplus48_p2.bin"
	.byte FF_LDY
	INCBIN "vendor/cdfjplus/cdfjplus48_p3.bin"
	.byte FF_OFFSET
	INCBIN "vendor/cdfjplus/cdfjplus48_p4.bin"
	IF * != $0800
	ECHO "CDFJ+ driver must be exactly 2048 bytes"
	ERR
	ENDIF

;@ Bank 0 code, at 6507 $1000.
	ORG $0800
	RORG $1000
	INCLUDE "kernel.asm"
	REND

	INCLUDE "vectors.asm"

;@ ARM C code, built by the ARM stage of the Makefile (starts at $2800).
	ORG $2800
	INCBIN "build/arm/armcode.bin"

	ORG $7FFF
	.byte $FF
