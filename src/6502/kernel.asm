;@ kernel.asm - bank 0 code (6507 $1000-$1FF3): boot, frame loop, visible kernel.
;@
;@ Frame (NTSC, 262 lines): VSYNC 3 | VBLANK ~37 | visible 192 | overscan ~30.
;@ The ARM coprocessor runs once per frame in VBLANK (CALLFN 255).
;@
;@ Visible kernel: one scanline per pass of the loop, 76 CPU cycles.
;@ Cycle numbers count from the start of the line (after WSYNC). The TIA
;@ color clock is 3x the CPU cycle, so a write that completes at cycle c lands at clock 3c.
;@
;@ Every line starts with HMOVE, so the sprite pair drifts by the HM value the
;@ ARM streams in (up to 7 px left or 8 px right per line). That is how one pair
;@ of sprites moves between objects down the screen without a repositioning
;@ line. HMOVE blanks the left 8 pixels of each line.
;@   GRP0/GRP1 land in HBLANK (clocks 24, 39)
;@   left  PF0/PF1/PF2 land at clocks 54, 69, 84   (needed by 68, 84, 116)
;@   right PF0/PF1/PF2 land at clocks 123, 138, 153 (windows 84-148, 116-164, 148-196)
;@   HM, COLUP, COLUBK and ENAM0 are written late in the line for the next line.
;@ The loop is exactly 76 cycles with no WSYNC; that is what makes room for ENAM0.

VBLANK_TIMER_60   = 43      ; TIM64T value for the VBLANK interval (NTSC)
OVERSCAN_TIMER_60 = 35      ; TIM64T value for the overscan interval (NTSC)

top_x             = $80     ; zero-page copy of the top sprite X fetched from the ARM
scene_mode        = $81     ; 0 = game kernel, 1 = scene kernel (set by the ARM each frame)

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
	lda #(FF_OFFSET+STREAM_MODE)    ; which visible kernel this frame uses
	sta scene_mode
	lda #(FF_OFFSET+STREAM_COLUPF)  ; fetch the frame's playfield color
	sta COLUPF

	; Sprite pair position at the top: P0 at top_x, P1 8 px to its right.
	lda #(FF_OFFSET+STREAM_TOPX)
	sta top_x
	ldx #0
	jsr PosObject
	lda top_x
	clc
	adc #8
	ldx #1
	jsr PosObject
	lda #(FF_OFFSET+STREAM_M0X)     ; the bullet
	ldx #2
	jsr PosObject
	sta WSYNC
	sta HMOVE                       ; 3: apply fine motion in HBLANK
	lda scene_mode
	bne .waitVBLANK                 ; the scene kernel reads each line's values itself
	; Game kernel only: entry 0 of the next-line tables: line 0's color,
	; background, bullet and motion.
	lda #(FF_OFFSET+STREAM_COLUP)   ; 5
	sta COLUP0                      ; 8
	sta COLUP1                      ; 11
	lda #(FF_OFFSET+STREAM_BK)      ; 13
	sta COLUBK                      ; 16
	lda #(FF_OFFSET+STREAM_ENAM0)   ; 18
	sta ENAM0                       ; 21
	lda #$20                        ; 23: one copy of P0, missile 4 px wide
	sta NUSIZ0                      ; 26
	lda #(FF_OFFSET+STREAM_HM)      ; 28
	sta HMP0                        ; 31: HMxx left alone for 24 cycles after HMOVE
	sta HMP1                        ; 34
	lda #0                          ; 36
	sta HMM0                        ; 39: the bullet does not drift with the sprites

.waitVBLANK:
	lda INTIM
	bne .waitVBLANK
	sta WSYNC                       ; start a line, then leave VBLANK
	lda #0
	sta VBLANK
	ldx #LINES
	lda scene_mode
	beq .gameKernel
	jmp SceneKernel
.gameKernel:
	sta WSYNC                       ; the loop below starts exactly at cycle 0

;@ Exactly 76 cycles per pass, with no WSYNC, so each pass is one scanline.
;@ The branch must not cross a page (checked below), or it takes a cycle more.
VisibleLoop:
	sta HMOVE                       ; 3: drift the sprite pair
	lda #(FF_OFFSET+STREAM_GRP0)    ; 5
	sta GRP0                        ; 8, clock 24 (HBLANK)
	lda #(FF_OFFSET+STREAM_GRP1)    ; 10
	sta GRP1                        ; 13, clock 39 (HBLANK)
	lda #(FF_OFFSET+STREAM_PF0L)    ; 15
	sta PF0                         ; 18, clock 54
	lda #(FF_OFFSET+STREAM_PF1L)    ; 20
	sta PF1                         ; 23, clock 69
	lda #(FF_OFFSET+STREAM_PF2L)    ; 25
	sta PF2                         ; 28, clock 84
	lda #(FF_OFFSET+STREAM_HM)      ; 30
	sta HMP0                        ; 33: next line's drift, >= 24 cycles after HMOVE
	sta HMP1                        ; 36
	lda #(FF_OFFSET+STREAM_PF0R)    ; 38
	sta PF0                         ; 41, clock 123
	lda #(FF_OFFSET+STREAM_PF1R)    ; 43
	sta PF1                         ; 46, clock 138
	lda #(FF_OFFSET+STREAM_PF2R)    ; 48
	sta PF2                         ; 51, clock 153
	lda #(FF_OFFSET+STREAM_COLUP)   ; 53
	sta COLUP0                      ; 56: next line's sprite color (ARM changes it
	sta COLUP1                      ; 59  only on lines with no sprite showing)
	lda #(FF_OFFSET+STREAM_BK)      ; 61
	sta COLUBK                      ; 64: next line's background (from pixel 124)
	lda #(FF_OFFSET+STREAM_ENAM0)   ; 66
	sta ENAM0                       ; 69: next line's bullet enable (from pixel 139)
	nop                             ; 71
	dex                             ; 73
	bne VisibleLoop                 ; 76 when taken: the next pass starts at cycle 0
VisibleLoopEnd:
	IF (>VisibleLoop) != (>VisibleLoopEnd)
	ECHO "VisibleLoop crosses a page; its branch would take 77 cycles"
	ERR
	ENDIF

;@ Falls through after the last visible line, at its cycle 75, so the next
;@ instruction starts the first overscan line (a WSYNC here would waste a line).
Overscan:
	lda #2
	sta VBLANK
	lda #0                          ; clear playfield, sprites and background, so the
	sta PF0                         ; next frame's top line is black instead of
	sta PF1                         ; last frame's floor
	sta PF2
	sta GRP0
	sta GRP1
	sta COLUBK
	sta ENAM0
	lda #OVERSCAN_TIMER_60
	sta TIM64T
.waitOverscan:
	lda INTIM
	bne .waitOverscan
	jmp FrameStart

;@ Scene kernel, for the intro and menus: one scanline per pass (WSYNC at the top).
;@ Nothing drifts or shoots here, which frees the cycles for a playfield color
;@ per line. Every table is indexed by the line it draws (entry k = line k).
;@   PF0/PF1/PF2 left land at clocks 15, 30, 99 (needed by 68, 84, 116)
;@   COLUPF and COLUBK land in HBLANK (45, 60)
;@   sprite color and graphics land at clocks 84, 114, 129, so a sprite must start
;@   at x >= 62 or its left part is drawn with the previous row's data
;@   PF0/PF1/PF2 right land at clocks 144, 159, 174 (windows 84-148, 116-164, 148-196)
SceneKernel:
SceneLoop:
	sta WSYNC                       ; cycle 0
	lda #(FF_OFFSET+STREAM_PF0L)    ; 2
	sta PF0                         ; 5
	lda #(FF_OFFSET+STREAM_PF1L)    ; 7
	sta PF1                         ; 10
	lda #(FF_OFFSET+STREAM_COLUPFL) ; 12
	sta COLUPF                      ; 15
	lda #(FF_OFFSET+STREAM_BK)      ; 17
	sta COLUBK                      ; 20
	lda #(FF_OFFSET+STREAM_COLUP)   ; 22
	sta COLUP0                      ; 25
	sta COLUP1                      ; 28
	lda #(FF_OFFSET+STREAM_PF2L)    ; 30
	sta PF2                         ; 33
	lda #(FF_OFFSET+STREAM_GRP0)    ; 35
	sta GRP0                        ; 38
	lda #(FF_OFFSET+STREAM_GRP1)    ; 40
	sta GRP1                        ; 43
	lda #(FF_OFFSET+STREAM_PF0R)    ; 45
	sta PF0                         ; 48
	lda #(FF_OFFSET+STREAM_PF1R)    ; 50
	sta PF1                         ; 53
	lda #(FF_OFFSET+STREAM_PF2R)    ; 55
	sta PF2                         ; 58
	dex                             ; 60
	bne SceneLoop                   ; 63
	sta WSYNC                       ; finish the last line; overscan starts on a line edge
	jmp Overscan

;@ Set the horizontal position of object X (0 = P0, 1 = P1, 2 = M0, 3 = M1) to A (0-159).
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
