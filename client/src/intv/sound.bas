' ===========================================================================
' sound.bas -- chords for board events on the PSG, played by the frame
' handler: the Atari's (src/atari/sound.s), as the Adam plays them
' (src/adam/sound.asm), its three notes on the PSG's three tone channels,
' faded together by the chord's rate. The game only starts a chord; nothing
' waits for one. A chord still sounding is cut off only by a higher-numbered
' one.
'
' The PSG's periods are the Adam's dividers: f = 3579545 / (32 * n) on both.
' (A PAL console's PSG runs at 4 MHz: a little sharp there.)
' ===========================================================================

    CONST CHORD_SPIKE = 0       ' any move: a short low chord
    CONST CHORD_BUY   = 1       ' your buy: two rising chords
    CONST CHORD_HIVE  = 2       ' a new hive
    CONST CHORD_JACK  = 3       ' a hivejack
    CONST CHORD_ICE   = 4       ' a hive becomes Black ICE
    CONST CHORD_TURN  = 5       ' your turn: two rising chords
    CONST CHORD_END   = 6       ' the null-route: a longer series
    CONST PSG_VOL_A   = $01FB   ' channel A's volume; B and C follow

    DIM snd_on                  ' 1 = sound on
    DIM sc                      ' play_chord's chord
    DIM sv                      ' the volume sounding, 0 = silent: the handler leaves the PSG alone
    DIM sl                      ' chord_load's chord (sc is the main loop's, sl both's)
    DIM srate, stick, snext, sprio, si, sn

' play_chord: chord sc, unless sound is off or a higher one is sounding.
play_chord: PROCEDURE
    IF snd_on = 0 THEN RETURN
    IF sv THEN IF sc < sprio THEN RETURN
    sprio = sc
    sv = 0                      ' the handler keeps out while it loads
    sl = sc
    GOSUB chord_load
END

' Sound on or off; on plays a chord, so the player hears it is on.
snd_toggle: PROCEDURE
    snd_on = snd_on XOR 1
    IF snd_on THEN sc = CHORD_HIVE : GOSUB play_chord : RETURN
    sv = 0
    POKE PSG_VOL_A, 0 : POKE PSG_VOL_A + 1, 0 : POKE PSG_VOL_A + 2, 0
END

' Chord sl's notes on the three channels, then its starting volume: sv last,
' as the handler plays it once sv isn't 0. From the main loop with sv = 0, or
' from the handler at the end of a chord with a next.
chord_load: PROCEDURE
    si = sl * 5
    SOUND 0, divider(chords(si)), 0
    SOUND 1, divider(chords(si + 1)), 0
    SOUND 2, divider(chords(si + 2)), 0
    sn = chords(si + 3)
    snext = chords(si + 4)
    srate = sn AND 15
    stick = srate
    sv = sn / 16
    POKE PSG_VOL_A, sv : POKE PSG_VOL_A + 1, sv : POKE PSG_VOL_A + 2, sv
END

' The frame handler's share: a volume step every srate frames; faded out, the
' next chord of a series, or quiet. (The volumes are written in place, not in
' a procedure: the stack is shallow.)
snd_isr: PROCEDURE
    IF sv = 0 THEN RETURN
    stick = stick - 1
    IF stick THEN RETURN
    stick = srate
    sv = sv - 1
    IF sv = 0 THEN IF snext <> 255 THEN sl = snext : GOSUB chord_load : RETURN
    POKE PSG_VOL_A, sv : POKE PSG_VOL_A + 1, sv : POKE PSG_VOL_A + 2, sv
END

' The notes: D3 E3 F3 G3 A3 B3 C4 D4 E4 F4 G4 A4
divider:
    DATA 762, 679, 641, 571, 508, 453, 428, 381, 339, 320, 285, 254
' Per chord: 3 notes, starting volume * 16 + frames per volume step, the next
' chord (255 = none). 0-6 are the CHORD_ numbers; the rest only follow on.
chords:
    DATA 0, 2, 4, $62, 255      ' 0 a spike: D minor, low, quiet and short
    DATA 6, 8, 10, $82, 7       ' 1 buy: C major...
    DATA 6, 8, 10, $A4, 255     ' 2 new hive: C major
    DATA 5, 7, 9, $A4, 255      ' 3 hivejack: B diminished
    DATA 1, 3, 5, $A6, 255      ' 4 Black ICE: E minor, low
    DATA 3, 5, 7, $82, 8        ' 5 your turn: G major...
    DATA 2, 4, 6, $A3, 9        ' 6 null-route: F major...
    DATA 6, 9, 11, $82, 255     ' 7 ...buy: up to F major, C held
    DATA 6, 8, 10, $82, 255     ' 8 ...your turn: up to C major
    DATA 6, 8, 10, $A3, 10      ' 9 ...null-route: C major...
    DATA 5, 9, 11, $AF, 255     ' 10 ...falls to B half-diminished (root, 5th, 7th)
