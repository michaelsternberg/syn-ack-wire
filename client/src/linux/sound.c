/* Sound for linux: a terminal has no tones, so the bell rings when it is your turn and at the null-route.
 * F3 turns it off and on (input.c calls snd_toggle). */
#include <curses.h>
#include "../platform.h"

static unsigned char on = 1;

void snd_init(void)
{
}

void __fastcall__ play_chord(unsigned char n)
{
    if (on && (n == CHORD_TURN || n == CHORD_END))
        beep();
}

void snd_toggle(void)
{
    on ^= 1;
}
