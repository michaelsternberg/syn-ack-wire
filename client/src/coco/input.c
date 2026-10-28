/* Keyboard and clock for the CoCo 1/2. Keys come from Color BASIC's keyboard
 * scan (POLCAT, through CMOC's inkey), which gives ASCII: ENTER is '\r', BREAK
 * 3 (Esc here), the left arrow 8 (Delete, K_DELETE already), the right arrow 9,
 * the up arrow '^'. The CoCo has no spare keys, so CLEAR stands in for the
 * Atari's OPTION (move hints), SHIFT+CLEAR (which types "\") turns sound on or
 * off, and the up arrow swaps green on black and black on green, both right
 * here, wherever the game is reading keys.
 *
 * The clock is Color BASIC's TIMER ($0112), which the 60 Hz interrupt counts.
 * fujinet-lib holds interrupts off while it talks to FujiNet, so it runs a
 * little slow, and the shot clock errs long; display_clock shows it a second
 * short, as on the Apple. */
#include <coco.h>
#include "../platform.h"

#define TIMER_LO (*(unsigned char *)0x0113)  /* TIMER's low byte */

void display_colours(void);             /* display_text.c */

unsigned char in_key(void)
{
    unsigned char k = inkey();
    switch (k) {
    case 0:
        return 0;
    case 3:                             /* BREAK */
        return K_ESC;
    case 9:                             /* right arrow: page turns in the instructions */
        return '>';
    case 12:                            /* CLEAR */
        return K_OPTION;
    case '\\':                          /* SHIFT+CLEAR */
        snd_toggle();
        return 0;
    case '^':                           /* up arrow */
        display_colours();
        return 0;
    case '?':
        return K_HELP;
    }
    if (k >= 'a' && k <= 'z')           /* SHIFT+0 locks lower case */
        k -= 'a' - 'A';
    return k;
}

unsigned char ticks(void)
{
    return TIMER_LO;
}

unsigned char ticks_per_sec(void)
{
    return 60;
}
