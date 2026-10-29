; sound.asm -- chords for board events on the Adam's SN76489 (port FFh), played
; by the NMI: the Atari's (src/atari/sound.s), its notes and envelopes, as the
; NES plays them (src/nes/sound.s), the three notes on the chip's three tone
; channels, faded together by the chord's rate. The program only starts a
; chord; the NMI plays it, so nothing waits.
;
; Dividers for the 3.58 MHz clock: f = 3579545 / (32 * n), within 3 cents.
;
; void snd_init(void)
; void play_chord(unsigned char n)
; void snd_toggle(void)

        SECTION code_user

        PUBLIC  _snd_init, _play_chord, _snd_toggle, _snd_on
        PUBLIC  snd_frame

        defc    CHORD_HIVE = 2  ; platform.h

; notes: indexes into dividers
        defc    D3 = 0
        defc    E3 = 1
        defc    F3 = 2
        defc    G3 = 3
        defc    A3 = 4
        defc    B3 = 5
        defc    C4 = 6
        defc    D4 = 7
        defc    E4 = 8
        defc    F4 = 9
        defc    G4 = 10
        defc    A4 = 11

dividers:
        defw    762, 679, 641, 571, 508, 453, 428, 381, 339, 320, 285, 254
; per chord: 3 notes, starting volume << 4 | frames per volume step, next chord
; ($FF = none). Entries 0-6 are the chord numbers in platform.h; the rest only
; follow on. As the Atari's.
chords: defb    D3, F3, A3, $62, $FF    ; 0 a spike: D minor, low, quiet and short
        defb    C4, E4, G4, $82, 7      ; 1 buy: C major...
        defb    C4, E4, G4, $A4, $FF    ; 2 new hive: C major
        defb    B3, D4, F4, $A4, $FF    ; 3 hivejack: B diminished
        defb    E3, G3, B3, $A6, $FF    ; 4 Black ICE: E minor, low
        defb    G3, B3, D4, $82, 8      ; 5 your turn: G major...
        defb    F3, A3, C4, $A3, 9      ; 6 null-route: F major...
        defb    C4, F4, A4, $82, $FF    ; 7 ...buy: up to F major, C held
        defb    C4, E4, G4, $82, $FF    ; 8 ...your turn: up to C major
        defb    C4, E4, G4, $A3, 10     ; 9 ...null-route: C major...
        defb    B3, F4, A4, $AF, $FF    ; 10 ...falls to B half-diminished (root, 5th, 7th)

_snd_init:
        ; fall through

; All four channels silent.
quiet:  ld      a,$9F
        out     ($FF),a
        ld      a,$BF
        out     ($FF),a
        ld      a,$DF
        out     ($FF),a
        ld      a,$FF
        out     ($FF),a
        ret

; Turning sound on plays a chord, so the player hears that it is on.
_snd_toggle:
        ld      a,(_snd_on)
        xor     1
        ld      (_snd_on),a
        jr      nz,toggled
        ld      (vol),a         ; off: silent at once
        jr      quiet
toggled:
        ld      a,CHORD_HIVE
        jr      chord_a

; Start chord n, unless sound is off or a higher-numbered chord is still sounding.
_play_chord:
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
chord_a:
        ld      c,a
        ld      a,(_snd_on)
        or      a
        ret     z
        ld      a,(vol)
        or      a
        jr      z,take
        ld      a,(prio)
        ld      b,a
        ld      a,c
        cp      b
        ret     c
take:   ld      a,c
        ld      (prio),a
        xor     a
        ld      (vol),a         ; the NMI keeps out while the chord loads
        ; fall through

; Load chord C, set its dividers and start its envelope. Called with vol = 0,
; or from the NMI.
load:   ld      a,c
        add     a,a
        add     a,a
        add     a,c             ; 5 bytes a chord
        ld      e,a
        ld      d,0
        ld      hl,chords
        add     hl,de
        ld      de,notes
        ld      bc,5
        ldir
        ld      c,$80           ; tone 0, then 1 ($A0), then 2 ($C0)
        ld      de,notes
ld_note:
        ld      a,(de)
        inc     de
        add     a,a
        push    de
        ld      e,a
        ld      d,0
        ld      hl,dividers
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)          ; de = n, 10 bits
        ld      a,e
        and     $0F
        or      c
        out     ($FF),a         ; the latch: channel and the low 4 bits
        ld      a,e
        srl     d
        rra
        srl     d
        rra
        srl     d
        rra
        srl     d
        rra
        and     $3F
        out     ($FF),a         ; the high 6 bits
        pop     de
        ld      a,c
        add     a,$20
        ld      c,a
        cp      $E0
        jr      nz,ld_note
        ld      a,(rate)
        ld      b,a
        and     $0F
        ld      (rate),a
        ld      (tick),a
        ld      a,b
        rrca
        rrca
        rrca
        rrca
        and     $0F             ; the starting volume
        ld      (vol),a         ; last: now the NMI plays it
        jp      put

; A frame of the chord (from the NMI): down a volume step every `rate`
; frames; faded out, the next chord of a series, or quiet.
snd_frame:
        ld      a,(vol)
        or      a
        ret     z
        ld      hl,tick
        dec     (hl)
        jr      nz,put
        ld      a,(rate)
        ld      (hl),a
        ld      hl,vol
        dec     (hl)
        jr      nz,put
        ld      a,(next)        ; faded out: the next chord of a series
        cp      $FF
        jp      z,quiet         ; none: silent
        ld      c,a
        jp      load
put:    ld      a,(vol)         ; the volume as the chip's attenuation, on all three
        cpl
        and     $0F
        ld      b,a
        or      $90
        out     ($FF),a
        ld      a,b
        or      $B0
        out     ($FF),a
        ld      a,b
        or      $D0
        out     ($FF),a
        ret

        SECTION data_user
_snd_on:        defb    1

        SECTION bss_user
vol:            defs    1       ; 0 = silent: the NMI leaves the chip alone
notes:          defs    3       ; the chord now sounding
rate:           defs    1       ;   starting volume << 4 | frames per volume step
next:           defs    1       ;   the chord after it, or $FF
tick:           defs    1       ; frames to the next volume step
prio:           defs    1       ; the chord number play_chord started
