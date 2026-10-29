/* Sound for the Heathkit H89, which has none of its own: the H19 terminal's bell (BEL), one short beep,
 * for the chords that ask the player to look: their turn, a hivejack, the null-route. The rest stay
 * quiet, as a beep for every spike would wear. */
#include "../platform.h"
#include "h89.h"

static unsigned char on;

void snd_init(void)
{
    on = 1;
}

void play_chord(unsigned char n)
{
    if (on && (n == CHORD_TURN || n == CHORD_JACK || n == CHORD_END))
        bdos(6, 0x07);
}

void snd_toggle(void)
{
    on ^= 1;
    if (on)
        bdos(6, 0x07);
}
