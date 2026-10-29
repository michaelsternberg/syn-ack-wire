; sound.s -- chords for board events, from the NMI: the Atari's (src/atari/
; sound.s), its notes and envelopes, on the APU's two pulse channels and its
; triangle. A chord's first note goes to the triangle an octave down, a bass
; under the other two on the pulses; the pulses fade by the chord's rate, and
; the triangle, which has no volume, stops once they are quiet. The program
; only starts a chord; the NMI plays it, so nothing waits.
;
; Periods for the NTSC clock (1.79 MHz): f = 1789773 / (16 * (t + 1)), within 3
; cents. A PAL machine's slower clock plays them about a semitone flat.
;
; void snd_init(void)
; void __fastcall__ play_chord(unsigned char n)
; void snd_toggle(void)

        .export         _snd_init, _play_chord, _snd_toggle, _snd_on
        .export         snd_nmi

CHORD_HIVE = 2                  ; platform.h
PULSE      = $B0                ; 50% duty, no length counter, constant volume | volume
TRI_ON     = $FF
TRI_OFF    = $80
TRI_QUIET  = 4                  ; below this pulse volume the triangle stops

; notes: indexes into periods
D3 = 0
E3 = 1
F3 = 2
G3 = 3
A3 = 4
B3 = 5
C4 = 6
D4 = 7
E4 = 8
F4 = 9
G4 = 10
A4 = 11

.segment "DATA"
_snd_on: .byte 1

.segment "ZEROPAGE"
vol:    .res 1                  ; 0 = silent: the NMI leaves the APU alone

.segment "BSS"
notes:  .res 3                  ; the chord now sounding
rate:   .res 1                  ;   starting volume << 4 | frames per volume step
next:   .res 1                  ;   the chord after it, or $FF
tick:   .res 1                  ; frames to the next volume step
prio:   .res 1                  ; the chord number play_chord started

.segment "RODATA"
periods:
        .word   761, 678, 640, 570, 507, 452, 427, 380, 338, 319, 284, 253
; per chord: 3 notes, starting volume << 4 | frames per volume step, next chord
; ($FF = none). Entries 0-6 are the chord numbers in platform.h; the rest only
; follow on. As the Atari's.
chords: .byte D3, F3, A3, $62, $FF    ; 0 a spike: D minor, low, quiet and short
        .byte C4, E4, G4, $82, 7      ; 1 buy: C major...
        .byte C4, E4, G4, $A4, $FF    ; 2 new hive: C major
        .byte B3, D4, F4, $A4, $FF    ; 3 hivejack: B diminished
        .byte E3, G3, B3, $A6, $FF    ; 4 Black ICE: E minor, low
        .byte G3, B3, D4, $82, 8      ; 5 your turn: G major...
        .byte F3, A3, C4, $A3, 9      ; 6 null-route: F major...
        .byte C4, F4, A4, $82, $FF    ; 7 ...buy: up to F major, C held
        .byte C4, E4, G4, $82, $FF    ; 8 ...your turn: up to C major
        .byte C4, E4, G4, $A3, 10     ; 9 ...null-route: C major...
        .byte B3, F4, A4, $AF, $FF    ; 10 ...falls to B half-diminished (root, 5th, 7th)

.segment "CODE"

_snd_init:
        lda     #$08            ; the sweeps off
        sta     $4001
        sta     $4005
        lda     #PULSE
        sta     $4000
        sta     $4004
        lda     #TRI_OFF
        sta     $4008
        lda     #$07            ; pulse 1, pulse 2, triangle
        sta     $4015
        rts

; Turning sound on plays a chord, so the player hears that it is on.
_snd_toggle:
        lda     _snd_on
        eor     #1
        sta     _snd_on
        bne     :+
        sta     vol             ; off: silent at once
        jsr     quiet
:       lda     #CHORD_HIVE
        ; fall through

; Start chord A, unless sound is off or a higher-numbered chord is still sounding.
_play_chord:
        ldx     _snd_on
        beq     done
        tax
        lda     vol
        beq     :+
        cpx     prio
        bcc     done
:       stx     prio
        lda     #0
        sta     vol             ; the NMI keeps out while the chord loads
        ; fall through

; Load chord X, set its periods and start its envelope. Called with vol = 0,
; or from the NMI.
load:   txa
        asl     a
        asl     a
        sta     tick
        txa
        adc     tick            ; 5 bytes a chord
        tay
        ldx     #0
:       lda     chords,y
        sta     notes,x
        iny
        inx
        cpx     #5
        bne     :-
        lda     notes           ; the triangle: the period of the pulse's note, an octave down
        asl     a
        tay
        lda     periods,y
        sta     $400A
        lda     periods+1,y
        ora     #$08
        sta     $400B
        lda     notes+1
        asl     a
        tay
        lda     periods,y
        sta     $4002
        lda     periods+1,y
        ora     #$08
        sta     $4003
        lda     notes+2
        asl     a
        tay
        lda     periods,y
        sta     $4006
        lda     periods+1,y
        ora     #$08
        sta     $4007
        lda     rate
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        tax                     ; the starting volume
        lda     rate
        and     #$0F
        sta     rate
        sta     tick
        stx     vol             ; last: now the NMI plays it
done:   rts

snd_nmi:
        lda     vol
        beq     @out
        dec     tick
        bne     @put
        lda     rate
        sta     tick
        dec     vol
        bne     @put
        ldx     next            ; faded out: the next chord of a series
        bmi     quiet           ; none: silent
        jsr     load
@put:   lda     vol
        ora     #PULSE
        sta     $4000
        sta     $4004
        ldx     #TRI_ON
        lda     vol
        cmp     #TRI_QUIET
        bcs     :+
        ldx     #TRI_OFF
:       stx     $4008
@out:   rts

quiet:  lda     #PULSE          ; volume 0
        sta     $4000
        sta     $4004
        lda     #TRI_OFF
        sta     $4008
        rts
