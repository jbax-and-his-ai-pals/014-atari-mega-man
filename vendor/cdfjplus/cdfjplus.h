;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; CDFJ Plus - Atari 2600 Bankswitching Include
; Chris Walton, Fred Quimby, Darrell Spice Jr, John Champeau
; (C) Copyright 2020
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	ifnconst FF_OFFSET
FF_OFFSET = 0
	endif

; Fetcher Constants
DS0DATA     = $0 + FF_OFFSET
DS1DATA     = $1 + FF_OFFSET
DS2DATA     = $2 + FF_OFFSET
DS3DATA     = $3 + FF_OFFSET
DS4DATA     = $4 + FF_OFFSET
DS5DATA     = $5 + FF_OFFSET
DS6DATA     = $6 + FF_OFFSET
DS7DATA     = $7 + FF_OFFSET
DS8DATA     = $8 + FF_OFFSET
DS9DATA     = $9 + FF_OFFSET
DS10DATA    = $A + FF_OFFSET
DS11DATA    = $B + FF_OFFSET
DS12DATA    = $C + FF_OFFSET
DS13DATA    = $D + FF_OFFSET
DS14DATA    = $E + FF_OFFSET
DS15DATA    = $F + FF_OFFSET
DS16DATA    = $10 + FF_OFFSET
DS17DATA    = $11 + FF_OFFSET
DS18DATA    = $12 + FF_OFFSET
DS19DATA    = $13 + FF_OFFSET
DS20DATA    = $14 + FF_OFFSET
DS21DATA    = $15 + FF_OFFSET
DS22DATA    = $16 + FF_OFFSET
DS23DATA    = $17 + FF_OFFSET
DS24DATA    = $18 + FF_OFFSET
DS25DATA    = $19 + FF_OFFSET
DS26DATA    = $1A + FF_OFFSET
DS27DATA    = $1B + FF_OFFSET
DS28DATA    = $1C + FF_OFFSET
DS29DATA    = $1D + FF_OFFSET
DS30DATA    = $1E + FF_OFFSET
DS31DATA    = $1F + FF_OFFSET
DSCOMM      = $20 + FF_OFFSET		; datastream used for DSPTR and DSWRITE
DSJMP1      = $21 + FF_OFFSET		; datastream used for JMP FASTJMP1
DSJMP2      = $22 + FF_OFFSET		; datastream used for JMP FASTJMP2
AMPLITUDE   = $23 + FF_OFFSET

; Mode Constants
FASTON      = $00
FASTOFF     = $0F
AUDIOSAMPLE = $00
AUDIOMUSIC  = $F0

; FastJmp Address
FASTJMP1    = $0000
FASTJMP2    = $0001

DSJMP       = DSJMP1        ; for backwards compaitibility
FASTJMP     = FASTJMP1      ; for backwards compatibility

  ; CDF Base Address
  IFNCONST CDF_BASE_ADDRESS
CDF_BASE_ADDRESS = $1FF0
  ENDIF

  ; CDF Registers
  SEG.U CDF_REGISTERS
  ORG CDF_BASE_ADDRESS

  ; Write Registers
DSWRITE     DS 1    ; $1FF0
DSPTR       DS 1    ; $1FF1
SETMODE     DS 1    ; $1FF2
CALLFN      DS 1    ; $1FF3

  ; Bankswitching Hotspots
BANK0       DS 1    ; $1FF4
BANK1       DS 1    ; $1FF5
BANK2       DS 1    ; $1FF6
BANK3       DS 1    ; $1FF7
BANK4       DS 1    ; $1FF8
BANK5       DS 1    ; $1FF9
BANK6       DS 1    ; $1FFA

  SEG

