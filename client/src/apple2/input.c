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

static unsigned char k, held, pressed;
static unsigned char now[8], sec = 0xFF, count, spin;

unsigned char in_key(void)
{
    /* The Apple keys aren't latched: they count only while held when this
     * looks, once a pass of the game loop, so a press counts on the first look
     * that sees it. Contact bounce is over long before the next look: there are
     * SmartPort calls between them. */
    k = (OAPPLE >> 7) | (CAPPLE >> 7 << 1);           /* bit 0 Open-Apple, bit 1 Closed-Apple */
    if (k != held) {
        pressed = k & ~held;
        held = k;
        if (pressed & 2)
            snd_toggle();
        if (pressed & 1)
            return K_OPTION;
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

/* Seconds since the first call, from FujiNet's clock (now[6] = seconds). The
 * clock is read on one call in CLOCK_EVERY, at most about 10 times a second: the
 * calls between return the last count, and each waits a moment (pause), so the
 * calls themselves keep time even in a loop that does nothing else. If the clock
 * doesn't answer (no clock device), every 4th read counts a second. */
#define CLOCK_EVERY 160                 /* x half a millisecond: a tenth of a second, at least; more
                                           apart, a second shows late enough to see the count stall */
void __fastcall__ pause(unsigned char n);   /* sound.s */
static unsigned cached;

unsigned char ticks(void)
{
    if (cached) {
        --cached;
        pause(2);
        return count;
    }
    cached = CLOCK_EVERY;
    if (clock_get_time(now, SIMPLE_BINARY) != 0) {
        if (!(++spin & 3))
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
