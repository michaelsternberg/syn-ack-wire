; snow.s -- static in the title's letters, like a TV with no signal.
; Four quad-width players side by side cover the art (one bit a character);
; a deferred VBI refills them from POKEY's RANDOM every frame, masked to the
; letters' strokes (title_art.inc, from tools/title_art.py). The OS skips the
; deferred VBI while SIO runs, so the static freezes for each transfer.
;
; void snow_init(void)
; unsigned char snow_on         0 = leave the players alone

        .export _snow_init, _snow_on
        .import _pm

RANDOM  = $D20A
VVBLKD  = $0224         ; deferred VBI vector
SETVBV  = $E45C

        .rodata
        .include "title_art.inc"        ; SNOW_TOP, SNOW_ROWS, snow_mask

P0      = _pm + $200 + SNOW_TOP         ; players 1-3 follow, $80 apart

        .bss
_snow_on: .res 1
oldvbi: .res 2

        .code
_snow_init:
        lda VVBLKD
        sta oldvbi
        lda VVBLKD+1
        sta oldvbi+1
        lda #7                  ; deferred
        ldy #<vbi
        ldx #>vbi
        jmp SETVBV

vbi:    lda _snow_on
        beq @out
        ldx #4*SNOW_ROWS-1      ; 4 player bytes a text row
@byte:  txa
        lsr a
        lsr a
        tay
        lda RANDOM
        and snow_mask,y
        sta P0,x
        lda RANDOM
        and snow_mask+SNOW_ROWS,y
        sta P0+$80,x
        lda RANDOM
        and snow_mask+2*SNOW_ROWS,y
        sta P0+$100,x
        lda RANDOM
        and snow_mask+3*SNOW_ROWS,y
        sta P0+$180,x
        dex
        bpl @byte
@out:   jmp (oldvbi)
