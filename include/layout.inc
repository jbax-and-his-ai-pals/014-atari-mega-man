; layout.inc - memory map shared by the 6507 kernel (DASM) and the ARM code (C).
; tools/gen_layout.py turns this file into build/gen/layout.h.
; Keep one "NAME = value" assignment per line; values are $hex or decimal.

; ARM-side addresses
DD_BASE_ARM   = $40000800   ; display data; datastream index 0 is its first byte
DRV_PTR_ARM   = $40000098   ; driver datastream pointers, 4 bytes each
DRV_INC_ARM   = $40000124   ; driver datastream increments, 4 bytes each

; Scanlines per frame that the kernel draws
LINES         = 192

; Small fields at the start of display data
DD_IN_SWCHA   = $000        ; 6507 -> ARM: SWCHA, P0 joystick in D7-D4 (0 = pressed)
DD_IN_INPT4   = $001        ; 6507 -> ARM: INPT4, P0 fire in D7 (0 = pressed)
DD_MODE       = $002        ; ARM -> 6507: 0 = game kernel, 1 = scene kernel
DD_IN_HEADROOM = $003       ; 6507 -> ARM: INTIM right after the last ARM call, VBLANK time left (64-cycle units)
DD_COLUPF     = $004        ; ARM -> 6507: playfield color for the frame (game kernel)
DD_TOPX0      = $005        ; ARM -> 6507: X of the first object on sprite channel 0 (P0)
DD_TOPX1      = $006        ; ARM -> 6507: X of the first object on sprite channel 1 (P1)
DD_COLUPFL    = $010        ; LINES+1 bytes: playfield color per line (scene kernel)

; Datastreams. The kernel reads stream n with LDA #(FF_OFFSET + n). Tables are
; indexed by scanline (entry k = line k) except HM0/HM1, whose entry 0 is read in
; VBLANK and whose entry k+1 is read at the end of line k for line k+1.
NUM_STREAMS   = 18
STREAM_PF0L   = 0
STREAM_PF1L   = 1
STREAM_PF2L   = 2
STREAM_PF0R   = 3
STREAM_PF1R   = 4
STREAM_PF2R   = 5
STREAM_GRP0   = 6           ; GRP0, one byte per scanline
STREAM_GRP1   = 7           ; GRP1
STREAM_COLUP0 = 8           ; COLUP0
STREAM_COLUP1 = 9           ; COLUP1
STREAM_HM0    = 10          ; HMP0: drift of P0 applied at the start of the line
STREAM_HM1    = 11          ; HMP1
STREAM_BK     = 12          ; COLUBK
STREAM_COLUPF = 13          ; COLUPF, one byte, read once per frame in VBLANK
STREAM_TOPX0  = 14          ; DD_TOPX0, read once per frame
STREAM_TOPX1  = 15          ; DD_TOPX1
STREAM_COLUPFL = 16         ; COLUPF per line (scene kernel only)
STREAM_MODE   = 17          ; DD_MODE, read once per frame

; Display data tables (byte offsets from DD_BASE_ARM, 192 bytes each unless noted)
DD_PF0L       = $100
DD_PF1L       = $1C0
DD_PF2L       = $280
DD_PF0R       = $340
DD_PF1R       = $400
DD_PF2R       = $4C0
DD_GRP0       = $580
DD_GRP1       = $640
DD_COLUP0     = $700        ; LINES+1 bytes
DD_COLUP1     = $800        ; LINES+1 bytes
DD_HM0        = $900        ; LINES+1 bytes
DD_HM1        = $A00        ; LINES+1 bytes
DD_BK         = $B00        ; LINES+1 bytes
DD_FB         = $C00        ; 960-byte framebuffer for the title and menu screens (see fb.h)
