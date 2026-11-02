; layout.inc - memory map shared by the 6507 kernel (DASM) and the ARM code (C).
; tools/gen_layout.py turns this file into build/gen/layout.h.
; Keep one "NAME = value" assignment per line; values are $hex or decimal.

; ARM-side addresses
DD_BASE_ARM   = $40000800   ; display data; datastream index 0 is its first byte
DRV_PTR_ARM   = $40000098   ; driver datastream pointers, 4 bytes each
DRV_INC_ARM   = $40000124   ; driver datastream increments, 4 bytes each

; Scanlines per frame that the kernel draws
LINES         = 192

; Inputs. Each frame the 6507 writes these with DSPTR/DSWRITE before CALLFN.
DD_IN_SWCHA   = $000        ; SWCHA: P0 joystick in D7-D4 (0 = pressed)
DD_IN_INPT4   = $001        ; INPT4: P0 fire in D7 (0 = pressed)

; Datastreams. The kernel reads stream n with LDA #(FF_OFFSET + n).
NUM_STREAMS   = 13
STREAM_PF0L   = 0
STREAM_PF1L   = 1
STREAM_PF2L   = 2
STREAM_PF0R   = 3
STREAM_PF1R   = 4
STREAM_PF2R   = 5
STREAM_BK     = 6           ; COLUBK, one byte per scanline
STREAM_GRP0   = 7           ; GRP0, one byte per scanline
STREAM_GRP1   = 8           ; GRP1, one byte per scanline
STREAM_COLUP0 = 9           ; COLUP0, one byte per scanline
STREAM_COLUP1 = 10          ; COLUP1, one byte per scanline
STREAM_COLUPF = 11          ; COLUPF, one byte, read once per frame in VBLANK
STREAM_HEROX  = 12          ; hero X (0-143, P0; P1 is 8 px right), once per frame

; Display data tables (byte offsets from DD_BASE_ARM, 192 bytes each unless noted)
DD_PF0L       = $100
DD_PF1L       = $1C0
DD_PF2L       = $280
DD_PF0R       = $340
DD_PF1R       = $400
DD_PF2R       = $4C0
DD_BK         = $580
DD_GRP0       = $640
DD_GRP1       = $700
DD_COLUP0     = $7C0
DD_COLUP1     = $880
DD_COLUPF     = $940        ; one byte
DD_HEROX      = $941        ; one byte
