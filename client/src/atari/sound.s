; sound.s -- chords for board events, from a deferred VBI.
; SIO borrows POKEY channels 3 and 4 as its serial clock, and each transfer
; turns every channel off. The OS skips the deferred VBI while SIO runs (CRITIC),
; so writing all four channels every frame never disturbs a transfer, and
; net_sio.s calls snd_put as soon as one ends to put the chord back. The chord
; is still silent during the transfer itself, so while a chord sounds net_poll
; skips its once-a-second status check and only reads when FujiNet has data.
; Both go through net_sio.s's net_hush (our volume lives there) and net_after,
; so the network code links without this file.
;
; All chords are from B Locrian, 8-bit dividers on the 64 kHz clock:
; f = 63921 / (2 * (AUDF + 1)), within 10 cents from E3 to A4.
;
; void snd_init(void)
; void __fastcall__ play_chord(unsigned char n)
; void snd_toggle(void)

        .export _snd_init, _play_chord, _snd_toggle
        .import net_hush, net_after

AUDF1   = $D200         ; AUDFn = AUDF1 + 2(n-1), AUDCn right after it
AUDC1   = $D201
AUDCTL  = $D208
VVBLKD  = $0224         ; deferred VBI vector
SETVBV  = $E45C

PURE    = $A0           ; AUDC: pure tone; low nibble = volume
CHORD_HIVE = 2          ; platform.h

; note dividers
D3 = 217
E3 = 193
F3 = 182
G3 = 162
A3 = 144
B3 = 128
C4 = 121
D4 = 108
E4 = 96
F4 = 91
G4 = 81
A4 = 72

        .data
_snd_on: .byte 1

vol     = net_hush      ; 0 = silent: the VBI leaves POKEY alone

        .bss
pitch:  .res 4          ; the chord now sounding: AUDF1-4, 0 = voice off
rate:   .res 1          ;   starting volume << 4 | frames per volume step
next:   .res 1          ;   the chord after it, or $FF
tick:   .res 1          ; frames to the next volume step
prio:   .res 1          ; the chord number play_chord started
oldvbi: .res 2

        .rodata
; per chord: 4 pitches, starting volume << 4 | frames per volume step, next
; chord ($FF = none). Four voices at volume 10 stay clear of clipping.
; Entries 0-6 are the chord numbers in platform.h; the rest only follow on.
chords: .byte D3, F3, A3, 0,   $62, $FF ; 0 a spike: D minor, low, quiet and short
        .byte C4, E4, G4, 0,   $82, 7   ; 1 buy: C major...
        .byte C4, E4, G4, 0,   $A4, $FF ; 2 new hive: C major
        .byte B3, D4, F4, 0,   $A4, $FF ; 3 hivejack: B diminished
        .byte E3, G3, B3, 0,   $A6, $FF ; 4 Black ICE: E minor, low
        .byte G3, B3, D4, 0,   $82, 8   ; 5 your turn: G major...
        .byte F3, A3, C4, 0,   $A3, 9   ; 6 null-route: F major...
        .byte C4, F4, A4, 0,   $82, $FF ; 7 ...buy: up to F major, C held
        .byte C4, E4, G4, 0,   $82, $FF ; 8 ...your turn: up to C major
        .byte C4, E4, G4, 0,   $A3, 10  ; 9 ...null-route: C major...
        .byte F3, B3, D4, A4,  $AF, $FF ; 10 ...falls to B half-diminished 7th

        .code

_snd_init:
        lda #<snd_put
        sta net_after
        lda #>snd_put
        sta net_after+1
        lda VVBLKD
        sta oldvbi
        lda VVBLKD+1
        sta oldvbi+1
        lda #7                  ; deferred
        ldy #<vbi
        ldx #>vbi
        jmp SETVBV

; Turning sound on plays a chord, so the player hears that it is on.
_snd_toggle:
        lda _snd_on
        eor #1
        sta _snd_on
        lda #CHORD_HIVE
        ; fall through

; Start chord A, unless sound is off or a higher-numbered chord is still sounding.
_play_chord:
        ldx _snd_on
        beq done
        tax
        lda vol
        beq :+
        cpx prio
        bcc done
:       stx prio
        lda #0
        sta vol                 ; the VBI keeps out while the chord loads
        ; fall through

; Load chord X and start its envelope. Called with vol = 0, or from the VBI.
load:   txa
        asl a
        sta tick
        asl a
        adc tick                ; 6 bytes a chord
        tay
        ldx #0
:       lda chords,y
        sta pitch,x
        iny
        inx
        cpx #6
        bne :-
        lda rate
        lsr a
        lsr a
        lsr a
        lsr a
        tax                     ; the starting volume
        lda rate
        and #$0F
        sta rate
        sta tick
        stx vol                 ; last: now the VBI plays it
done:   rts

vbi:    lda vol
        beq @out
        dec tick
        bne @put
        lda rate
        sta tick
        dec vol
        bne @put
        ldx next                ; faded out: the next chord of a series
        bmi @put                ; none: write volume 0 once, then stay out
        jsr load
@put:   jsr snd_put
@out:   jmp (oldvbi)

; Write the chord to POKEY: the VBI every frame, net_sio.s after each transfer.
snd_put:
        lda #0
        sta AUDCTL
        ldx #3
        ldy #6
@voice: lda pitch,x
        sta AUDF1,y
        beq :+                  ; no note: AUDC 0
        lda vol
        ora #PURE
:       sta AUDC1,y
        dey
        dey
        dex
        bpl @voice
        rts
