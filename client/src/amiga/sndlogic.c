#include "../platform.h"
#include "sndlogic.h"

/* The speaker chords of the PC and Apple II, as Hz: the same pitches and lengths. */
const unsigned short amiga_chord_notes[7][3][2] = {
    { { 459,  44 } },                                   /* SPIKE: a short low click */
    { { 669,  52 }, { 832,  54 } },                     /* BUY: two rising */
    { { 832,  60 }, { 1100, 64 } },                     /* HIVE */
    { { 905,  50 }, { 628,  56 } },                     /* JACK: falling */
    { { 404,  62 }, { 404,  62 } },                     /* ICE: two low knocks */
    { { 715,  63 }, { 993,  60 } },                     /* TURN: two rising */
    { { 628,  96 }, { 769,  98 }, { 993, 111 } }        /* END: a longer series */
};

unsigned char amiga_chord_wins(unsigned char playing, unsigned char incoming)
{
    return playing == SND_NONE || incoming > playing;
}

unsigned char amiga_snd_request(struct snd_state *s, unsigned char chord, unsigned char now, unsigned char tps)
{
    unsigned short ms = 0;
    unsigned char n;

    if (!s->on || chord > CHORD_END)
        return SND_NONE;
    if (s->playing != SND_NONE && (unsigned char)(s->end - now) < 128 && !amiga_chord_wins(s->playing, chord))
        return SND_NONE;
    for (n = 0; n < 3 && amiga_chord_notes[chord][n][0]; ++n)
        ms += amiga_chord_notes[chord][n][1];
    s->playing = chord;
    s->end = now + (unsigned char)((unsigned long)ms * tps / 1000 + 1);
    return chord;
}

unsigned char amiga_snd_toggle(struct snd_state *s, unsigned char now, unsigned char tps)
{
    s->on ^= 1;
    if (!s->on)
        return SND_NONE;
    s->playing = SND_NONE;
    return amiga_snd_request(s, CHORD_HIVE, now, tps);
}

unsigned short amiga_note_period(unsigned short hz, unsigned long clock)
{
    return (unsigned short)((clock + hz * 4UL) / (hz * 8UL));
}

unsigned short amiga_note_cycles(unsigned short hz, unsigned short ms)
{
    unsigned short n = (unsigned short)((unsigned long)hz * ms / 1000);
    return n ? n : 1;
}
