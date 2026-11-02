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
; Tables marked "next line" have LINES+1 entries: entry 0 is read in VBLANK and
; takes effect on line 0; entry k+1 is read at the end of line k for line k+1.
NUM_STREAMS   = 15
STREAM_PF0L   = 0
STREAM_PF1L   = 1
STREAM_PF2L   = 2
STREAM_PF0R   = 3
STREAM_PF1R   = 4
STREAM_PF2R   = 5
STREAM_GRP0   = 6           ; GRP0, one byte per scanline
STREAM_GRP1   = 7           ; GRP1, one byte per scanline
STREAM_HM     = 8           ; HMP0 = HMP1 (sprite drift via HMOVE), next line
STREAM_COLUP  = 9           ; COLUP0 = COLUP1, next line
STREAM_BK     = 10          ; COLUBK, next line
STREAM_COLUPF = 11          ; COLUPF, one byte, read once per frame in VBLANK
STREAM_TOPX   = 12          ; X of the sprite pair at the top of the frame (P0; P1 is 8 px right)
STREAM_ENAM0  = 13          ; ENAM0 (bullet on this line), next line
STREAM_M0X    = 14          ; X of missile 0 (the bullet), once per frame

; Display data tables (byte offsets from DD_BASE_ARM, 192 bytes each unless noted)
DD_PF0L       = $100
DD_PF1L       = $1C0
DD_PF2L       = $280
DD_PF0R       = $340
DD_PF1R       = $400
DD_PF2R       = $4C0
DD_GRP0       = $580
DD_GRP1       = $640
DD_HM         = $700        ; LINES+1 bytes
DD_COLUP      = $800        ; LINES+1 bytes
DD_BK         = $900        ; LINES+1 bytes
DD_COLUPF     = $A00        ; one byte
DD_TOPX       = $A01        ; one byte
DD_M0X        = $A02        ; one byte
DD_ENAM0      = $B00        ; LINES+1 bytes
