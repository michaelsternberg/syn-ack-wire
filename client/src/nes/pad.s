; pad.s -- the joypad, read by the NMI every frame, so a press is never lost
; while the program waits on the FujiNet: what was newly pressed goes into an
; 8-event ring that input.c empties (the NMI writes pad_head, the program
; pad_tail).
;
; An event is the buttons newly pressed in a frame (A $80, B $40, SELECT $20,
; START $10, up $08, down $04, left $02, right $01). SELECT is a shift: a press
; of another button while it is held comes with $20 set, and SELECT alone
; makes its own event, $20, when it is let go without one.
;
; Port 1 is read on bits 0 and 1 both, so a Famicom's expansion-port pad works
; as well as its own.
        .export         _pad_now, _pad_ring, _pad_head, _pad_tail
        .export         pad_nmi

JOY1      = $4016
SEL       = $20

.segment "LORAM"
_pad_ring:   .res 8
_pad_head:   .res 1
_pad_tail:   .res 1
_pad_now:    .res 1             ; the buttons held, as the last frame found them

.segment "ZEROPAGE"
prev:        .res 1
new:         .res 1
chord:       .res 1             ; a button went with this SELECT

.segment "CODE"

pad_nmi:
        lda     _pad_now
        sta     prev
        lda     #1
        sta     JOY1
        lsr     a
        sta     JOY1
        ldx     #8
@bit:   lda     JOY1
        and     #3
        cmp     #1              ; carry = either bit
        rol     _pad_now
        dex
        bne     @bit

        lda     prev
        eor     #$FF
        and     _pad_now
        and     #<~SEL
        sta     new
        lda     _pad_now
        and     #SEL
        beq     @up
        lda     prev            ; SELECT held: a new press of it starts a shift
        and     #SEL
        bne     @shift
        stx     chord           ; (x = 0)
@shift: lda     new
        beq     @out
        ora     #SEL
        sta     chord
        bne     @push
@up:    lda     prev            ; SELECT not held: let go just now, alone?
        and     #SEL
        beq     @plain
        lda     chord
        bne     @plain
        lda     #SEL
        jsr     @push
@plain: lda     new
        beq     @out
@push:  ldx     _pad_head       ; A: the event
        sta     _pad_ring,x
        inx
        txa
        and     #7
        cmp     _pad_tail
        beq     @out            ; full: dropped
        sta     _pad_head
@out:   rts
