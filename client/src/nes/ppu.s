; ppu.s -- the NMI's share of drawing the screen: what changed in the program's
; copy of the two name tables reaches the PPU in the vertical blank.
;
; The program draws into txt_cells (the game's name table, $2000) and
; help_cells (the instructions', $2400): 30 rows of 32 tiles each. When a row
; is written it sets the row's mark in txt_dirty (0-29 the game's, 30-59 the
; instructions') and then any_dirty; an attribute row (2 of the 4-row
; palette bands) is written into attr and then marked in attr_dirty. Marks
; are set only after the writes, and the NMI clears a mark before it copies,
; so a row written again meanwhile is simply copied again.
;
; The vertical blank holds about 2270 cycles; the sprites' DMA takes 514. So a
; frame copies at most ROWS_MAX rows of 32 (about 450 cycles each), one fewer
; when attribute rows (ATTR_MAX of 8) come too:
; measured in MAME (a write tap on the NMI's first store and on PPUMASK), 4
; rows ran to 2570 cycles, past the blank. ppu_after picks the rows after the
; blank, when time no longer matters; the blank only copies them. A full screen
; takes 10 frames, in view: rows drawn over themselves don't flicker. The cells
; start on a page (game.cfg's CELLS), so no row's (ptr),y crosses one.
;
; The title's static is the 4 tiles $0C-$0F, both planes random (half their
; pixels colours 1 and 3, half dark): 64 bytes a frame from a different place
; in a table of random bytes. It pauses while rows are waiting, as the Atari's
; pauses for SIO, so it never takes a blank from them.
        .export         _txt_cells, _help_cells, _txt_dirty, _attr, _attr_dirty
        .export         _any_dirty, _noise_on, _noise
        .export         ppu_nmi, ppu_after

PPUSTATUS = $2002
PPUADDR   = $2006
PPUDATA   = $2007

ROWS      = 30                  ; a name table's rows
ROWS_MAX  = 3
ATTR_MAX  = 2
NOISE_TILE = $0C

.segment "CELLS"
_txt_cells:  .res 32 * ROWS
_help_cells: .res 32 * ROWS

.segment "BSS"
_attr:       .res 128           ; the game's attribute table, then the instructions'
_noise:      .res 256           ; random bytes (text.c fills it)

.segment "LORAM"
_txt_dirty:  .res 2 * ROWS
_attr_dirty: .res 16            ; 8 attribute rows a name table
_any_dirty:  .res 1             ; a row may be marked
_noise_on:   .res 1             ; the title's static runs
noise_go:    .res 1             ; ...this frame: no rows to copy
q_n:         .res 1             ; rows queued for the next blank
q_srcl:      .res ROWS_MAX
q_srch:      .res ROWS_MAX
q_dstl:      .res ROWS_MAX
q_dsth:      .res ROWS_MAX

.segment "ZEROPAGE"
nptr:        .res 2
nx:          .res 1
noise_at:    .res 1
row:         .res 1
budget:      .res 1
base_lo:     .res 1
base_hi:     .res 1
nt_hi:       .res 1

.segment "CODE"

; In the vertical blank: the attribute rows, the static, the queued rows.
ppu_nmi:
        bit     PPUSTATUS
        ldy     #ATTR_MAX
        ldx     #0
@attr:  lda     _attr_dirty,x
        beq     @anext
        lda     #0
        sta     _attr_dirty,x
        stx     nx
        txa                     ; $23C0 + (x & 7) * 8, or $27C0 for the instructions'
        and     #8
        lsr     a
        ora     #$23
        sta     PPUADDR
        txa
        asl     a
        asl     a
        asl     a
        tax                     ; x * 8: where it is in attr
        and     #$38
        ora     #$C0
        sta     PPUADDR
        .repeat 8
        lda     _attr,x
        sta     PPUDATA
        inx
        .endrepeat
        ldx     nx
        dey
        beq     @noise
@anext: inx
        cpx     #16
        bne     @attr

@noise: lda     noise_go
        beq     @rows
        lda     #>(NOISE_TILE * 16)
        sta     PPUADDR
        lda     #<(NOISE_TILE * 16)
        sta     PPUADDR
        ldx     noise_at
        ldy     #8
@nz:    .repeat 8
        lda     _noise,x
        sta     PPUDATA
        inx
        .endrepeat
        dey
        bne     @nz

@rows:  ldx     #0
@row:   cpx     q_n
        bne     :+
        jmp     @done
:
        lda     q_dsth,x
        sta     PPUADDR
        lda     q_dstl,x
        sta     PPUADDR
        lda     q_srcl,x
        sta     nptr
        lda     q_srch,x
        sta     nptr+1
        ldy     #0
        .repeat 32
        lda     (nptr),y
        sta     PPUDATA
        iny
        .endrepeat
        inx
        jmp     @row
@done:  lda     #0
        sta     q_n
        rts

; After the blank: the static's next place, and the rows for the next frame.
ppu_after:
        lda     noise_at        ; an 8-bit LFSR: never 0, so it starts at 1
        bne     :+
        lda     #1
:       asl     a
        bcc     :+
        eor     #$1D
:       sta     noise_at

        lda     _any_dirty
        bne     :+
        lda     _noise_on       ; nothing waiting: the static's turn
        sta     noise_go
        rts
:       lda     #0              ; rows may be waiting: the static waits
        sta     noise_go
        lda     #ROWS_MAX
        ldx     #15             ; an attribute row to copy costs a row
@ad:    ldy     _attr_dirty,x
        bne     @less
        dex
        bpl     @ad
        bmi     @set
@less:  sec
        sbc     #1
@set:   sta     budget
        ldy     #0
        ldx     #0
@scan:  lda     _txt_dirty,x
        beq     @next
        lda     #0
        sta     _txt_dirty,x
        txa
        cmp     #ROWS
        bcs     @help
        sta     row
        lda     #<_txt_cells
        sta     base_lo
        lda     #>_txt_cells
        sta     base_hi
        lda     #$20
        bne     @queue
@help:  sbc     #ROWS           ; carry is set
        sta     row
        lda     #<_help_cells
        sta     base_lo
        lda     #>_help_cells
        sta     base_hi
        lda     #$24
@queue: sta     nt_hi
        lda     row             ; the row's 32 bytes: row * 32 = (row >> 3) : (row << 5)
        lsr     a
        lsr     a
        lsr     a
        sta     nx
        ora     nt_hi
        sta     q_dsth,y
        lda     row
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        sta     q_dstl,y
        clc
        adc     base_lo
        sta     q_srcl,y
        lda     nx
        adc     base_hi
        sta     q_srch,y
        iny
        cpy     budget
        beq     @full
@next:  inx
        cpx     #2 * ROWS
        bne     @scan
        lda     #0              ; every mark was looked at
        sta     _any_dirty
@full:  sty     q_n
@out:   rts
