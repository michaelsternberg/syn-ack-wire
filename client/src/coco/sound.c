/* Chords for board events on the CoCo 1/2: square waves from the 6-bit DAC
 * (PIA1 port A, $FF20 bits 7-2) through the analog switch to the TV. A chord
 * is up to three short notes in a row, timed by a delay loop, as on the Apple
 * (src/apple2/sound.s, whose table this is); it blocks while it plays, well
 * under half a second, and FujiNet holds the bytes that arrive meanwhile.
 *
 * $FF20's bit 1 is the serial port's output, FujiNet's line: every write keeps
 * bits 1-0 as they were.
 *
 * A note's half period is about 5 * pitch + 22 cycles of the 0.89 MHz clock,
 * so pitch 100 is about 860 Hz. The pitches are the Apple's times 7/8, less 2,
 * for the same notes. */
#include <coco.h>
#include "../platform.h"

#define NOTES 3                         /* a chord's notes; pitch 0 ends it early */
#define VOLUME 0xFC                     /* the DAC swings 0 to 63 */

static unsigned char on, pitch, halves;

/* per chord (platform.h order), NOTES pitches then NOTES lengths (half periods) */
static const unsigned char chords[] = {
    190,   0,   0,  40,   0,   0,       /* SPIKE: a short low click */
    129, 103,   0,  70,  90,   0,       /* BUY: two rising */
    103,  77,   0, 100, 140,   0,       /* HIVE */
     94, 138,   0,  90,  70,   0,       /* JACK: falling */
    217, 217,   0,  50,  50,   0,       /* ICE: two low knocks */
    120,  86,   0,  90, 120,   0,       /* TURN: two rising */
    138, 112,  86, 120, 150, 220,       /* END: a longer series */
};

void snd_init(void)
{
    on = 1;
}

void snd_toggle(void)
{
    on ^= 1;
    if (on)
        play_chord(CHORD_HIVE);
}

void __fastcall__ play_chord(unsigned char n)
{
    const unsigned char *c = chords + n * (2 * NOTES);
    unsigned char k;

    if (!on)
        return;
    asm {
        lda     $FF01                   ; the analog switch to the DAC: PIA0 CA2 and CB2 low
        anda    #$F7
        sta     $FF01
        lda     $FF03
        anda    #$F7
        sta     $FF03
        lda     $FF23                   ; sound on: PIA1 CB2 high
        ora     #$08
        sta     $FF23
    }
    for (k = 0; k < NOTES && c[k]; ++k) {
        pitch = c[k];
        halves = c[k + NOTES];
        asm {
            lda     $FF20
            anda    #$03                ; the serial line and the cassette bit, as they were
@half       eora    #$FC                ; VOLUME
            sta     $FF20
            ldb     _pitch
@wait       decb
            bne     @wait
            dec     _halves
            bne     @half
        }
    }
    asm {
        lda     $FF20                   ; the DAC back to 0, then sound off
        anda    #$03
        sta     $FF20
        lda     $FF23
        anda    #$F7
        sta     $FF23
    }
}
