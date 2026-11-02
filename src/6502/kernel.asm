;@ kernel.asm - bank 0 code (6507 $1000-$1FF3): boot, frame loop, visible kernel.
;@
;@ Frame (NTSC, 262 lines): VSYNC 3 | VBLANK ~37 | visible 192 | overscan ~30.
;@ The ARM coprocessor runs once per frame in VBLANK (CALLFN 255).
;@
;@ Visible kernel: one scanline per pass of the loop, 76 CPU cycles.
;@ Cycle numbers count from the start of the line (after WSYNC). The TIA
;@ color clock is 3x the CPU cycle, so a write that completes at cycle c lands at clock 3c.
;@   left  PF0/PF1/PF2 land at clocks 15, 45, 60  (needed by 68, 84, 116)
;@   right PF0/PF1/PF2 land at clocks 144, 159, 174 (needed by 148, 164, 196)
;@ Sprite graphics and colors land before the hero's pixels (clock 75-120).

VBLANK_TIMER_60   = 43      ; TIM64T value for the VBLANK interval (NTSC)
OVERSCAN_TIMER_60 = 35      ; TIM64T value for the overscan interval (NTSC)

hero_x            = $80     ; zero-page copy of the hero X fetched from the ARM

Reset:
	sei
	cld
	ldx #$FF
	txs
	ldx #0
	txa
.clear:
	sta $00,x                       ; zero TIA and RIOT RAM
	inx
	bne .clear
	lda #(AUDIOSAMPLE|FASTON)       ; fast fetch on, no audio mode (as the template starts)
	sta SETMODE

FrameStart:
	sta WSYNC                       ; finish the overscan line, so VSYNC starts at a line edge
	lda #2
	sta VSYNC
	sta WSYNC
	sta WSYNC
	sta WSYNC
	lda #0
	sta VSYNC

	lda #2
	sta VBLANK
	lda #VBLANK_TIMER_60
	sta TIM64T
	; Inputs -> display data (DD_IN_SWCHA, DD_IN_INPT4) for the ARM to read.
	; Two DSPTR writes set the write offset (0 here), then DSWRITE stores and
	; advances one byte at a time.
	lda #0
	sta DSPTR
	sta DSPTR
	lda SWCHA
	sta DSWRITE
	lda INPT4
	sta DSWRITE

	lda #255
	sta CALLFN                      ; ARM: game update; returns when done
	lda #(FF_OFFSET+STREAM_COLUPF)  ; fetch the frame's playfield color
	sta COLUPF

	; Hero position: P0 at hero_x, P1 8 px to its right.
	lda #(FF_OFFSET+STREAM_HEROX)
	sta hero_x
	ldx #0
	jsr PosObject
	lda hero_x
	clc
	adc #8
	ldx #1
	jsr PosObject
	sta WSYNC
	sta HMOVE                       ; apply fine motion in HBLANK

.waitVBLANK:
	lda INTIM
	bne .waitVBLANK
	sta WSYNC                       ; start a line, then leave VBLANK
	lda #0
	sta VBLANK
	ldx #LINES

VisibleLoop:
	sta WSYNC                       ; cycle 0 of the visible line
	lda #(FF_OFFSET+STREAM_PF0L)    ; cycles 0-1
	sta PF0                         ; cycles 2-4, lands at clock 15
	lda #(FF_OFFSET+STREAM_BK)      ; cycles 5-6
	sta COLUBK                      ; cycles 7-9, lands at clock 30
	lda #(FF_OFFSET+STREAM_PF1L)    ; cycles 10-11
	sta PF1                         ; cycles 12-14, lands at clock 45
	lda #(FF_OFFSET+STREAM_PF2L)    ; cycles 15-16
	sta PF2                         ; cycles 17-19, lands at clock 60
	lda #(FF_OFFSET+STREAM_GRP0)    ; cycles 20-21
	sta GRP0                        ; cycles 22-24, lands at clock 75
	lda #(FF_OFFSET+STREAM_GRP1)    ; cycles 25-26
	sta GRP1                        ; cycles 27-29, lands at clock 90
	lda #(FF_OFFSET+STREAM_COLUP0)  ; cycles 30-31
	sta COLUP0                      ; cycles 32-34, lands at clock 105
	lda #(FF_OFFSET+STREAM_COLUP1)  ; cycles 35-36
	sta COLUP1                      ; cycles 37-39, lands at clock 120
	bit $00                         ; cycles 40-42: wait for the right-half window
	lda #(FF_OFFSET+STREAM_PF0R)    ; cycles 43-44
	sta PF0                         ; cycles 45-47, lands at clock 144
	lda #(FF_OFFSET+STREAM_PF1R)    ; cycles 48-49
	sta PF1                         ; cycles 50-52, lands at clock 159
	lda #(FF_OFFSET+STREAM_PF2R)    ; cycles 53-54
	sta PF2                         ; cycles 55-57, lands at clock 174
	dex                             ; cycles 58-59
	bne VisibleLoop                 ; cycles 60-62; the next WSYNC begins by cycle 63

;@ Falls through after the last visible line.
Overscan:
	sta WSYNC
	lda #2
	sta VBLANK
	lda #0                          ; clear playfield, sprites and background, so the
	sta PF0                         ; next frame's top line is black instead of
	sta PF1                         ; last frame's floor
	sta PF2
	sta GRP0
	sta GRP1
	sta COLUBK
	lda #OVERSCAN_TIMER_60
	sta TIM64T
.waitOverscan:
	lda INTIM
	bne .waitOverscan
	jmp FrameStart

;@ Set the horizontal position of object X (0 = P0, 1 = P1) to A (0-159).
;@ Coarse position: one RESPx strobe after a divide-by-15 loop; the remainder
;@ becomes the fine motion in HMPx. Takes one scanline; HMOVE must follow.
PosObject:
	sec
	sta WSYNC
.divide:
	sbc #15
	bcs .divide
	eor #7
	asl
	asl
	asl
	asl
	sta HMP0,x
	sta RESP0,x
	rts

	IF * > $1FF3
	ECHO "kernel overflows bank 0 (6507 $1FF3)"
	ERR
	ENDIF
