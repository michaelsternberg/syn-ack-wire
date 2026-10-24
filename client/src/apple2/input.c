/* Keyboard and clock for the Apple II (IIe, IIc, IIgs). Keys come from the
 * keyboard latch. Open-Apple and Closed-Apple pressed on their own act like the
 * Atari's OPTION and SELECT: Open-Apple is reported (move hints), Closed-Apple
 * turns sound on or off right here, wherever the game is reading keys.
 *
 * The clock is FujiNet's, in whole seconds, so ticks_per_sec() is 1. Counting
 * video frames would run slow whenever a SmartPort call blocks past one, and
 * the shot clock would then promise more time than the server gives. Whole
 * seconds can only err the other way: a countdown's first second is short. */
#include <fujinet-clock.h>
#include "../platform.h"

#define KBD     (*(volatile unsigned char *)0xC000)   /* bit 7 = a key */
#define KBDSTRB (*(volatile unsigned char *)0xC010)
#define OAPPLE  (*(volatile unsigned char *)0xC061)   /* bit 7 = held */
#define CAPPLE  (*(volatile unsigned char *)0xC062)

#define SETTLE  64                      /* polls to ignore the Apple keys after a change: contact bounce */

static unsigned char k, held, pressed, settle;
static unsigned char now[8], sec = 0xFF, count, spin;

unsigned char in_key(void)
{
    if (settle)
        --settle;
    else {
        k = (OAPPLE >> 7) | (CAPPLE >> 7 << 1);       /* bit 0 Open-Apple, bit 1 Closed-Apple */
        if (k != held) {
            settle = SETTLE;
            pressed = k & ~held;
            held = k;
            if (pressed & 2)
                snd_toggle();
            if (pressed & 1)
                return K_OPTION;
        }
    }
    k = KBD;
    if (!(k & 0x80))
        return 0;
    KBDSTRB = 0;
    k &= 0x7F;
    if (k == 0x7F)                      /* Delete; ← is $08, K_DELETE already */
        return K_DELETE;
    if (k == '?')
        return K_HELP;
    if (k >= 'a')
        k -= 'a' - 'A';
    return k;
}

/* Seconds since the first call, from FujiNet's clock (now[6] = seconds). If
 * the clock doesn't answer (no clock device), every 256th call counts one. */
unsigned char ticks(void)
{
    if (clock_get_time(now, SIMPLE_BINARY) != 0) {
        if (!++spin)
            ++count;
        return count;
    }
    k = now[6];
    if (sec != 0xFF)
        count += k < sec ? k + 60 - sec : k - sec;
    sec = k;
    return count;
}

unsigned char ticks_per_sec(void)
{
    return 1;
}
