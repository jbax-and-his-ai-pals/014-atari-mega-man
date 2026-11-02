;@ kernel.asm - bank 0 code (6507 $1000-$1FF3): boot, frame loop, visible kernel.
;@
;@ Frame (NTSC, 262 lines): VSYNC 3 | VBLANK ~37 | visible 192 | overscan ~30.
;@ The ARM coprocessor runs once per frame in VBLANK (CALLFN 255).
;@
;@ Game kernel: one scanline per pass of the loop, exactly 76 CPU cycles, no WSYNC.
;@ Cycle numbers count from the start of the line. The TIA color clock is 3x the CPU
;@ cycle, so a write that completes at cycle c lands at clock 3c.
;@
;@ Two fully independent sprite channels: P0 and P1 each stream their own graphics,
;@ color and horizontal drift (HMOVE on every line), so each can hop from object to
;@ object down the screen on its own. HMOVE blanks the left 8 pixels of each line.
;@   left  PF0/PF1/PF2 land at clocks 24, 39, 99 (needed by 76, 84, 116)
;@   COLUBK lands in HBLANK (54); GRP0/GRP1 at clocks 69, 84; COLUP0/COLUP1 at 114, 129
;@   right PF0/PF1/PF2 land at clocks 144, 159, 174 (windows 84-148, 116-164, 148-196)
;@   HMP0/HMP1 are written at the end of the line for the next line's HMOVE.
;@ There are no missiles: bullets are playfield cells (the ARM draws them), which
;@ is what frees the cycles for the second channel.
;@ Limits that follow from the write times: a P1 sprite left of x=16 would show the
;@ previous row's shape, and P0/P1 left of x=46/61 show the previous row's color for
;@ their left part, so the ARM keeps P1 objects at x >= 16.

VBLANK_TIMER_60   = 43      ; TIM64T value for the VBLANK interval (NTSC)
OVERSCAN_TIMER_60 = 35      ; TIM64T value for the overscan interval (NTSC)

scene_mode        = $81     ; 0 = game kernel, 1 = scene kernel (set by the ARM each frame)
headroom          = $82     ; INTIM after the ARM call; the ARM reads it next frame

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
	lda INTIM                       ; the VBLANK timer after the ARM's time is charged to it
	sta headroom                    ; (>= 128 means it already ran out)
	lda #(FF_OFFSET+STREAM_MODE)    ; which visible kernel this frame uses
	sta scene_mode
	lda #(FF_OFFSET+STREAM_COLUPF)  ; fetch the frame's playfield color
	sta COLUPF

	; Each channel starts at its first object's X.
	lda #(FF_OFFSET+STREAM_TOPX0)
	ldx #0
	jsr PosObject
	lda #(FF_OFFSET+STREAM_TOPX1)
	ldx #1
	jsr PosObject
	sta WSYNC
	sta HMOVE                       ; 3: apply fine motion in HBLANK
	lda #>DD_IN_HEADROOM            ; hand the ARM the headroom reading for next frame
	sta DSPTR
	lda #<DD_IN_HEADROOM
	sta DSPTR
	lda headroom
	sta DSWRITE
	lda scene_mode
	bne .waitVBLANK                 ; the scene kernel reads each line's values itself
	; Game kernel only: entry 0 of the drift tables, for line 0's HMOVE. HMxx must
	; be left alone for 24 cycles after HMOVE; the work above takes longer.
	lda #(FF_OFFSET+STREAM_HM0)
	sta HMP0
	lda #(FF_OFFSET+STREAM_HM1)
	sta HMP1

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
	sta HMOVE                       ; 3: drift each channel by its streamed amount
	lda #(FF_OFFSET+STREAM_PF0L)    ; 5
	sta PF0                         ; 8, clock 24
	lda #(FF_OFFSET+STREAM_PF1L)    ; 10
	sta PF1                         ; 13, clock 39
	lda #(FF_OFFSET+STREAM_BK)      ; 15
	sta COLUBK                      ; 18, clock 54 (HBLANK)
	lda #(FF_OFFSET+STREAM_GRP0)    ; 20
	sta GRP0                        ; 23, clock 69
	lda #(FF_OFFSET+STREAM_GRP1)    ; 25
	sta GRP1                        ; 28, clock 84
	lda #(FF_OFFSET+STREAM_PF2L)    ; 30
	sta PF2                         ; 33, clock 99
	lda #(FF_OFFSET+STREAM_COLUP0)  ; 35
	sta COLUP0                      ; 38, clock 114
	lda #(FF_OFFSET+STREAM_COLUP1)  ; 40
	sta COLUP1                      ; 43, clock 129
	lda #(FF_OFFSET+STREAM_PF0R)    ; 45
	sta PF0                         ; 48, clock 144
	lda #(FF_OFFSET+STREAM_PF1R)    ; 50
	sta PF1                         ; 53, clock 159
	lda #(FF_OFFSET+STREAM_PF2R)    ; 55
	sta PF2                         ; 58, clock 174
	lda #(FF_OFFSET+STREAM_HM0)     ; 60
	sta HMP0                        ; 63: next line's drift, >= 24 cycles after HMOVE
	lda #(FF_OFFSET+STREAM_HM1)     ; 65
	sta HMP1                        ; 68
	bit $00                         ; 71 (padding; before DEX so DEX sets the flags)
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
	lda #OVERSCAN_TIMER_60
	sta TIM64T
.waitOverscan:
	lda INTIM
	bne .waitOverscan
	jmp FrameStart

;@ Scene kernel, for the intro and menus: one scanline per pass (WSYNC at the top).
;@ Nothing drifts here, which frees the cycles for a playfield color per line, and
;@ only P0 is used. Every table is indexed by the line it draws (entry k = line k).
;@   PF0/PF1/PF2 left land at clocks 15, 30, 105 (needed by 76, 84, 116)
;@   COLUPF, COLUBK and COLUP0 land in HBLANK (45, 60, 75); GRP0 at clock 90, so a
;@   sprite must start at x >= 22 or its left part shows the previous row's shape
;@   PF0/PF1/PF2 right land at clocks 120, 135, 156 (windows 84-148, 116-164, 148-196)
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
	lda #(FF_OFFSET+STREAM_COLUP0)  ; 22
	sta COLUP0                      ; 25
	lda #(FF_OFFSET+STREAM_GRP0)    ; 27
	sta GRP0                        ; 30
	lda #(FF_OFFSET+STREAM_PF2L)    ; 32
	sta PF2                         ; 35
	lda #(FF_OFFSET+STREAM_PF0R)    ; 37
	sta PF0                         ; 40
	lda #(FF_OFFSET+STREAM_PF1R)    ; 42
	sta PF1                         ; 45
	nop                             ; 47: keeps PF2R clear of the last left-half cell
	lda #(FF_OFFSET+STREAM_PF2R)    ; 49
	sta PF2                         ; 52
	dex                             ; 54
	bne SceneLoop                   ; 57
	sta WSYNC                       ; finish the last line; overscan starts on a line edge
	jmp Overscan

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
