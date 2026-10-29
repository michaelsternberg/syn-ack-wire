/* Keyboard and clock for the Heathkit H89. Keys come from the H19 through CP/M (BDOS 6). The H19's
 * function keys stand in for the Atari's console keys, as on MS-DOS: f1 the instructions (as HELP, and ?
 * as on the others), f2 the move hints (OPTION), f3 sound on or off (SELECT), switched right here,
 * wherever the game is reading keys. They and the arrows send ESC and a letter, f1-f5 ESC S-W and the
 * arrows ESC A-D, both bytes at once; ESC alone is the ESC key.
 *
 * Every key check first sends the H19 what changed on the screen (display_flush).
 *
 * The clock is the BIOS's count of the H89's 2 ms interrupt (h89.h), which runs on while a call to the
 * FujiNet card waits. */
#include <stdlib.h>
#include "../platform.h"
#include "h89.h"

#define SEQ_TICKS 3                     /* how long the letter after ESC may take: ~50 ms */

void display_flush(void);               /* display_text.c: the changed cells to the H19 */
void display_restore(void);             /* display_text.c: the H19 as CP/M had it */

unsigned char in_key(void)
{
    unsigned char c, t0;
    display_flush();
    c = bdos(6, 0xFF);
    if (!c)
        return 0;
    if (c == K_ESC) {
        t0 = TICK;
        while (!(c = bdos(6, 0xFF)))
            if ((unsigned char)(TICK - t0) >= SEQ_TICKS)
                return K_ESC;
        switch (c) {
        case 'S': return K_HELP;        /* f1 */
        case 'T': return K_OPTION;      /* f2 */
        case 'U': snd_toggle(); return 0;               /* f3 */
        case 'D': return '<';           /* left and right: page turns in the instructions */
        case 'C': return '>';
        }
        return 0;
    }
    if (c == '?')
        return K_HELP;
    if (c == 0x7F)                      /* DELETE */
        return K_DELETE;
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    return c;                           /* RETURN is '\r', BACK SPACE '\b': the platform.h codes */
}

/* Esc and Y on the title screen: the H19 as CP/M had it, and back to CP/M. */
void platform_quit(void)
{
    display_restore();
    exit(0);
}

unsigned char ticks(void)
{
    return TICK;
}

unsigned char ticks_per_sec(void)
{
    return TICKS_PER_SEC;
}
