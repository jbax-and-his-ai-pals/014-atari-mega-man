;@ vectors.asm - CDFJ+ header words and 6507 vectors (bank 0, file $17F4-$17FF).

;@ Stella reads these from the file, not through the 6507. They sit at
;@ $1FF4-$1FFB, which are bank-switch hotspots, so the 6507 never reads them.
	ORG $17F4
	RORG $1FF4
	.byte $DC,$1F,$00,$40       ; C stack pointer: $40001FDC (little endian)
	.byte $01,$28,$00,$00       ; C entry: $2800 with the Thumb bit set
	REND

	ORG $17FC
	RORG $1FFC
	.word Reset                 ; RESET
	.word Reset                 ; IRQ/BRK (unused)
	REND
