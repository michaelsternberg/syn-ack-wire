; help.s -- the instruction pages for the Atari, read from D1 with raw SIO.
; No DOS, no files: tools/help_disk.py writes page n as screen codes in the 8
; sectors from HELP_SECTOR + 8*(n-1). The first holds the last 64 screen bytes and
; a signature, the other 7 the first 896. The page bar (row 24) is part of each
; page, so this code knows nothing about how many pages there are: '>' on the
; last page reads past the end, finds no signature, and stays put.
;
; void help_show(unsigned char (*key)(void))

        .export _help_show
        .importzp ptr1

        .ifndef HELP_SECTOR
HELP_SECTOR = 600       ; make passes the same number to help_disk.py
        .endif

DDEVIC  = $0300
DBUFLO  = $0304
DBUFHI  = $0305
DSTATS  = $0303
DAUX1   = $030A
DAUX2   = $030B
SIOV    = $E459
SAVMSC  = $58           ; screen memory

K_RETURN = $0D          ; platform.h
K_HELP  = $02
K_ESC   = $1B

        .bss
page:   .res 1          ; the page on screen; 0 = none yet
want:   .res 1
count:  .res 1
buf:    .res 128        ; a page's first sector

        .rodata
dcb:    .byte $31, 1, 'R', $40, 0, 0, 7, 0, 128, 0   ; D1: read 128 bytes, 7 s timeout
sig:    .byte "SYNACK"
        .define NODISK "NO HELP DISK IN D1"
nodisk: .repeat .strlen(NODISK), i
        .byte .strat(NODISK, i) - $20 + $80             ; inverse screen codes
        .endrepeat
NODISK_LEN = * - nodisk

        .code
_help_show:
        sta key+1
        stx key+2
        lda #0
        sta page
        lda #1
        jsr show
        bcc keys
        lda SAVMSC              ; no page 1: clear the screen and say why
        sta ptr1
        lda SAVMSC+1
        sta ptr1+1
        ldx #4                  ; 4 x 240 = 960
clear:  lda #0
        ldy #240
:       dey
        sta (ptr1),y
        bne :-
        lda ptr1
        clc
        adc #240
        sta ptr1
        bcc :+
        inc ptr1+1
:       dex
        bne clear
        ldy #NODISK_LEN-1
:       lda nodisk,y
        ora #$80
        sta (SAVMSC),y
        dey
        bpl :-

keys:   jsr key
        ldx page
        cmp #'>'
        beq next
        cmp #'*'                ; the arrow on the same key
        beq next
        cmp #'<'
        beq prev
        cmp #'+'
        beq prev
        cmp #K_ESC
        beq done
        cmp #K_RETURN
        beq done
        cmp #K_HELP
        bne keys
done:   rts
prev:   cpx #2
        bcc keys
        dex
        dex
next:   inx
        txa
        jsr show
        jmp keys

key:    jmp $FFFF               ; the caller's key function

; Show page A, if it's on the disk. Carry clear if it is now on screen.
show:   sta want
        sec
        sbc #1
        asl a
        asl a
        asl a
        clc
        adc #<HELP_SECTOR
        sta DAUX1
        lda #>HELP_SECTOR
        adc #0
        sta DAUX2
        lda #<buf
        ldx #>buf
        jsr read
        bmi fail
        ldx #5
:       lda buf+64,x
        cmp sig,x
        bne fail
        dex
        bpl :-
        lda SAVMSC
        sta ptr1
        lda SAVMSC+1
        sta ptr1+1
        lda #7
        sta count
:       lda ptr1
        ldx ptr1+1
        jsr read
        bmi fail                ; half a page; rare, and the next key redraws
        lda ptr1
        eor #$80                ; += 128
        sta ptr1
        bmi :+
        inc ptr1+1
:       dec count
        bne :--
        ldy #63                 ; the last 64 bytes, from the first sector
:       lda buf,y
        sta (ptr1),y
        dey
        bpl :-
        lda want
        sta page
        clc
        rts
fail:   sec
        rts

; Read the sector in DAUX1/2 into A/X, then step DAUX1/2 on. N set on an error.
read:   pha
        ldy #9
:       lda dcb,y
        sta DDEVIC,y
        dey
        bpl :-
        pla
        sta DBUFLO
        stx DBUFHI
        jsr SIOV
        inc DAUX1
        bne :+
        inc DAUX2
:       ldy DSTATS
        rts
