; help.s -- the instruction pages for the Atari, read from D1 with raw SIO.
; No DOS, no files: tools/help_disk.py writes page n as screen codes in the 8
; sectors from HELP_SECTOR + 8*(n-1). The first holds the last 64 screen bytes and
; a signature, the other 7 the first 896. The page bar (row 24) is part of each
; page, so this code knows nothing about how many pages there are: '>' on the
; last page reads past the end, finds no signature, and stays put.
;
; The pages have a screen of their own: opening them points the display list at it
; and closing points it back, so the game screen stays as it was, and keeps up
; with the game underneath while the pages are up (they are a mode of the
; caller's loop, which goes on reading the network).
;
; void help_open(void)                                  the page last read (it's
;                                                       still in the screen), else
;                                                       page 1, or why not
; unsigned char __fastcall__ help_key(unsigned char k)  a key for the pages: 0 =
;                                                       they closed

        .export _help_open, _help_key
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
SAVMSC  = $58           ; the game screen
SDLSTL  = $0230         ; the OS display list: its LMS address at 4
COLOR2  = $02C6
COLPF2  = $D018
GRAFP0  = $D00D         ; GRAFP0-3: what a player shows without DMA
GRACTL  = $D01D
NMIEN   = $D40E

K_RETURN = $0D          ; platform.h
K_HELP  = $02
K_ESC   = $1B

        .segment "HELP"         ; 1K-aligned, so it can't cross a 4K boundary
screen: .res 960

        .bss
page:   .res 1          ; the page in the screen; 0 = none (yet, or a torn read)
want:   .res 1
count:  .res 1
buf:    .res 128        ; a page's first sector

        .rodata
dcb:    .byte $31, 1, 'R', $40, 0, 0, 7, 0, 128, 0   ; D1: read 128 bytes, 7 s timeout
sig:    .byte "SYNACK"
        .define NODISK "NO HELP DISK IN D1. ESC:EXIT"
nodisk: .repeat .strlen(NODISK), i
        .byte .strat(NODISK, i) - $20 + $80             ; inverse screen codes
        .endrepeat
NODISK_LEN = * - nodisk

        .code
_help_open:
        lda page
        bne flip                ; back to it, with no reading
        lda #1                  ; read it while the game still shows
        jsr show
        bcc flip
        lda #0                  ; no page 1: a blank screen that says why
        ldx #240
:       dex
        sta screen,x
        sta screen+240,x
        sta screen+480,x
        sta screen+720,x
        bne :-
        ldy #NODISK_LEN-1
:       lda nodisk,y
        sta screen,y
        dey
        bpl :-
flip:   lda #0
        sta GRACTL              ; hide the players (the panel, the title's static)
        ldx #3
:       sta GRAFP0,x
        dex
        bpl :-
        lda #$40
        sta NMIEN               ; and the color bands: VBI only
        lda COLOR2
        sta COLPF2              ; the VBI may not get to it this frame
        lda #<screen
        ldx #>screen
        jmp lms

_help_key:
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
        bne up
done:   lda #2                  ; players back
        sta GRACTL
        lda #$C0                ; DLIs back
        sta NMIEN
        lda SAVMSC              ; and the game screen
        ldx SAVMSC+1
        jsr lms
        lda #0
        tax
        rts
prev:   cpx #2
        bcc up
        dex
        dex
next:   inx
        txa
        jsr show
up:     lda #1
        ldx #0
        rts

; Point the display list's LMS at A/X.
lms:    pha
        lda SDLSTL
        sta ptr1
        lda SDLSTL+1
        sta ptr1+1
        ldy #5
        txa
        sta (ptr1),y
        dey
        pla
        sta (ptr1),y
        rts

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
        lda #<screen
        sta ptr1
        lda #>screen
        sta ptr1+1
        lda #7
        sta count
:       lda ptr1
        ldx ptr1+1
        jsr read
        bmi torn                ; half a page; rare, and the next key redraws
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
torn:   lda #0
        sta page                ; not one to come back to
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
