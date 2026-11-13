/* Keyboard and clock for linux. Keys come from curses through nl_keymap (F1 help, F2 hints, F3 sound, F4 40/80
 * columns, F5 colour, as on MS-DOS), and every call also paints what the display drew, so the game loop needs
 * nothing of its own. The clock is the monotonic clock at 50 a second. */
#include <stdlib.h>
#include <time.h>
#include "../platform.h"
#include "keys.h"
#include "screen.h"


unsigned char in_key(void)
{
    int c;
    unsigned char k;
    nl_refresh();
    c = nl_read();
    if (c < 0) {                        /* nothing pressed: don't spin the CPU while the game waits */
        struct timespec ts = { 0, 2000000L };
        nanosleep(&ts, NULL);
        return 0;
    }
    k = nl_keymap(c);
    switch (k) {
    case NL_KEY_SOUND:  snd_toggle(); return 0;
    }
    return k;
}

/* Esc and Y on the title screen: silence, the terminal as it was, and the program ends. */
void platform_quit(void)
{
    nl_restore();
    exit(0);
}

/* The game's waits (dialling again, the pause between dials) only call ticks(), never in_key(): so a new tick is
 * also when the screen is painted and a signal looked at, and a call that finds the same tick sleeps a moment
 * instead of spinning. */
unsigned char ticks(void)
{
    static unsigned char last;
    static char first = 1;
    struct timespec ts;
    unsigned char t;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t = (unsigned char)((ts.tv_sec * 1000L + ts.tv_nsec / 1000000L) / 20);   /* 50 a second */
    if (first || t != last) {
        first = 0;
        last = t;
        nl_refresh();
    } else {
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000L;
        nanosleep(&ts, NULL);
    }
    return t;
}

unsigned char ticks_per_sec(void)
{
    return 50;
}
