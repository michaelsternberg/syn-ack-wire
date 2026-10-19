/* Test hooks for an unattended client (make test builds build/synack-auto.xex):
 * auto_key() stands in for in_key(): it types a handle, then presses keys a
 * player might press for each prompt, wrong ones included, so the local checks
 * and the server's refusals both get exercised. When a new prompt opens, and
 * every DUMP_TICKS, it executes BRK; atari800 -bbrk stops in its monitor there, and tools/run-test.sh feeds
 * the monitor commands that save screen memory to a file. */
#include "../src/platform.h"
#include "../src/common/game.h"

#ifndef DUMP_TICKS
#define DUMP_TICKS 240             /* 0: no screen dumps */
#endif

static unsigned char seed = 0x5A, last, prev, shown = 0xFF, typed;  /* 0xFF: dump the title too */
static unsigned elapsed;
static const char name[] = "ATARI\r";

static unsigned char rnd(void)
{
    seed = (seed << 1) ^ ((seed & 0x80) ? 0x1D : 0);   /* 8-bit LFSR */
    return seed;
}

unsigned char auto_key(void)
{
    unsigned char t = ticks(), r;
    elapsed += (unsigned char)(t - prev);
    prev = t;
    if (DUMP_TICKS && (elapsed >= DUMP_TICKS || pr_kind != shown)) {
        elapsed = 0;
        shown = pr_kind;
#ifdef __CC65__
        __asm__ ("brk");
        __asm__ ("nop");
#endif
    }
    in_key();
    if ((unsigned char)(t - last) < 20)             /* a third of a second between keys */
        return 0;
    last = t;
    if (name[typed])                                /* title: the handle, then Return */
        return name[typed++];
    r = rnd();
    switch (pr_kind) {
    case 'S':
        return '3';
    case 'P':
        return '1' + (r & 7);                       /* 7 and 8 are ignored */
    case 'F':
    case 'K':
        return syn_letter[r % NSYN];
    case 'D':
        return (r & 3) ? '0' + (r & 3) : K_RETURN;
    case 'B':
        if (r < 40)
            return K_DELETE;
        if (r < 100)
            return K_RETURN;
        if (r < 120)
            return 'N';
        return syn_letter[r % NSYN];
    }
    return 0;
}
