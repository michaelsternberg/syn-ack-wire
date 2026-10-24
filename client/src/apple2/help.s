; help.s -- the instruction pages for the Apple II, on text page 2 ($0800).
; The game always draws on page 1, so it keeps up underneath while the pages
; are up, and is simply there again when they close. The pages are one ProDOS
; file, SYNACK.HLP, made by tools/help_apple2.py: page n is the 1024 bytes from
; (n-1) * 1024, an image of text page 2 (screen holes and all). A page is read
; straight onto the screen with the ProDOS MLI: no cc65 file code. Page 2 keeps
; the last page read, so the pages open again where they were left. This code
; knows nothing about how many there are: '>' past the last finds no page there
; and stays put.
;
; The program starts above page 2 (game.cfg), and display_text.c turns 80STORE
; off, without which PAGE2 would select auxiliary memory.
;
; A page with LORES in its first screen hole ($0878, never shown) is a lo-res
; page in mixed mode: 40 x 40 blocks of graphics (a QR code) over 4 lines of
; text. The mode follows the page on screen: on opening and after each page
; turn. Closing always goes back to text, so the game screen comes back as it
; was.
;
; void help_open(void)
; unsigned char __fastcall__ help_key(unsigned char k)  0 = they closed

        .export _help_open, _help_key

MLI     = $BF00
TXTCLR  = $C050         ; graphics (lo-res here)
TXTSET  = $C051         ; text
MIXSET  = $C053         ; graphics with 4 lines of text under them
PAGE1   = $C054
PAGE2   = $C055
SCREEN2 = $0800
LORES   = $4C           ; tools/help_apple2.py: a lo-res page
HOLE    = SCREEN2 + $78
OPEN    = $C8           ; MLI calls
READ    = $CA
CLOSE   = $CC
SETMARK = $CE

K_DELETE = $08          ; platform.h; also the left arrow
K_HELP  = $02
K_RETURN = $0D
K_ESC   = $1B
K_RIGHT = $15           ; the right arrow

        .segment "PAGES"
iobuf:  .res 1024       ; ProDOS's buffer while the file is open (page-aligned)

        .rodata
path:   .byte 10, "SYNACK.HLP"
nofile: .byte "NO HELP FILE (SYNACK.HLP). ESC: BACK"
NOFILE_LEN = * - nofile

        .data
open_p: .byte 3
        .addr path, iobuf
open_r: .byte 0         ; ref_num, out
mark_p: .byte 2
mark_r: .byte 0
mark:   .byte 0, 0, 0   ; position: (page - 1) * 1024
read_p: .byte 4
read_r: .byte 0
        .addr SCREEN2
        .word 1024      ; asked for
got:    .word 0         ; read
close_p: .byte 1
close_r: .byte 0

        .bss
page:   .res 1          ; the page on screen 2; 0 = none yet
want:   .res 1

        .code
; The page last read, else page 1, else a line saying there is no file.
_help_open:
        lda page
        bne show
        lda #1
        jsr load
        bcc show
        ldx #0                  ; clear page 2 and say why
        lda #' ' | $80
:       sta SCREEN2,x
        sta SCREEN2+$100,x
        sta SCREEN2+$200,x
        sta SCREEN2+$300,x
        inx
        bne :-
        ldx #NOFILE_LEN - 1
:       lda nofile,x
        ora #$80
        sta SCREEN2,x
        dex
        bpl :-
show:   jsr mode
        bit PAGE2
        rts

; Text, or lo-res in mixed mode if the page on screen 2 says so.
mode:   lda HOLE
        cmp #LORES
        bne :+
        bit TXTCLR
        bit MIXSET
        rts
:       bit TXTSET
        rts

_help_key:
        cmp #K_ESC
        beq close
        cmp #K_RETURN
        beq close
        cmp #K_HELP
        beq close
        ldx page
        cmp #'>'
        beq next
        cmp #K_RIGHT
        beq next
        cmp #'<'
        beq prev
        cmp #K_DELETE
        bne stay
prev:   cpx #2
        bcc stay
        dex
        txa
        jsr load
        jmp turned
next:   inx
        txa
        jsr load
turned: jsr mode                ; the page on screen now, turned or not
stay:   lda #1
        ldx #0
        rts
close:  bit TXTSET
        bit PAGE1
        lda #0
        tax
        rts

; Read page A onto screen 2. Carry clear if it was all there; page 2 is
; untouched when the page isn't in the file at all.
load:   sta want
        jsr MLI
        .byte OPEN
        .addr open_p
        bcs @out
        lda open_r
        sta mark_r
        sta read_r
        sta close_r
        lda want                ; (want - 1) * 1024: the middle byte is (want - 1) * 4
        sec
        sbc #1
        asl
        asl
        sta mark+1
        jsr MLI
        .byte SETMARK
        .addr mark_p
        bcs @close
        jsr MLI
        .byte READ
        .addr read_p
        bcs @close
        lda got+1
        cmp #>1024
        bne @short
        lda want
        sta page
        clc
        bcc @close              ; always
@short: sec
@close: php
        jsr MLI
        .byte CLOSE
        .addr close_p
        plp
@out:   rts
