; sound.s -- chords for board events on the Apple II speaker. The speaker only
; clicks ($C030 flips it), so a chord is up to three short notes in a row, each
; a square wave timed by a delay loop. It blocks while it plays (well under half
; a second, at most); FujiNet holds the bytes that arrive meanwhile.
;
; A note's half period is about 5 * pitch + 15 cycles of the 1.023 MHz clock,
; so pitch 100 is about 990 Hz, 200 about 505 Hz.
;
; void snd_init(void)
; void __fastcall__ play_chord(unsigned char n)
; void snd_toggle(void)
;
; And the delay loop the network and the clock pace their polling with
; (net_fn.c, input.c), since the Apple has no timer:
; void __fastcall__ pause(unsigned char n)   n (1-255) units of about 256 cycles,
;                                            a quarter millisecond at 1 MHz

        .export _snd_init, _play_chord, _snd_toggle, _pause

SPKR    = $C030
CHORD_HIVE = 2          ; platform.h
NOTES   = 3             ; a chord's notes; pitch 0 ends it early

        .bss
on:     .res 1
note:   .res 1
pitch:  .res 1

        .rodata
; per chord (platform.h order), NOTES pitches then NOTES lengths (half periods)
chords: .byte 220,   0,   0,  40,   0,   0     ; SPIKE: a short low click
        .byte 150, 120,   0,  70,  90,   0     ; BUY: two rising
        .byte 120,  90,   0, 100, 140,   0     ; HIVE
        .byte 110, 160,   0,  90,  70,   0     ; JACK: falling
        .byte 250, 250,   0,  50,  50,   0     ; ICE: two low knocks
        .byte 140, 100,   0,  90, 120,   0     ; TURN: two rising
        .byte 160, 130, 100, 120, 150, 220     ; END: a longer series

        .code
_snd_init:
        lda #1
        sta on
        rts

_snd_toggle:
        lda on
        eor #1
        sta on
        beq done
        lda #CHORD_HIVE
        ; fall through: on plays the HIVE chord

_play_chord:
        ldx on
        beq done
        asl                     ; n * 6
        sta note
        asl
        adc note
        sta note                ; the chord's first pitch
        lda #NOTES
@next:  pha
        ldx note
        lda chords,x
        beq @end
        sta pitch
        ldy chords+NOTES,x      ; length in half periods
@half:  bit SPKR
        ldx pitch
@wait:  dex
        bne @wait
        dey
        bne @half
        inc note
        pla
        sec
        sbc #1
        bne @next
        rts
@end:   pla
done:   rts

_pause: tay
@unit:  ldx #50                 ; 50 x 5 cycles, and the 6 around them
:       dex
        bne :-
        dey
        bne @unit
        rts
