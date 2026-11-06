/* Test hooks for an unattended client (make test builds build/atari/synack-auto.xex):
 * auto_key() stands in for in_key(): it types a handle, then presses keys a
 * player might press for each prompt, wrong ones included, so the local checks
 * and the server's refusals both get exercised. When a new prompt opens, and
 * every DUMP_TICKS, it executes BRK; atari800 -bbrk stops in its monitor there, and tools/run-test.sh feeds
 * the monitor commands that save screen memory to a file. On a PC (make pc-apple2) it calls pc_dump()
 * instead, which saves the screen array; on the NES, where a BRK would only go to the IRQ handler, it
 * calls test/nes_dump.c's, which the emulator watches (tools/nes.lua). */
#include <string.h>
#include "../src/platform.h"
#include "../src/common/game.h"

#ifndef DUMP_TICKS
#define DUMP_TICKS 240             /* 0: no screen dumps */
#endif
#ifndef KEY_TICKS
#define KEY_TICKS 20               /* between keys: a third of a second at 60 ticks a second */
#endif

static unsigned char seed = 0x5A, last, prev, shown = 0xFF, typed, paged;  /* 0xFF: dump the title too */
static unsigned elapsed;
#ifndef AUTO_HANDLE
#define AUTO_HANDLE "ATARI"
#endif
static const char name[] = AUTO_HANDLE "\r";
#if REGION_PICK
/* on the title, the region panel: open it, read the seats again, pick the second region (and play there) */
static const unsigned char regioning[] = { K_REGION, 'R', '2', 0 };
static unsigned char regioned;
#endif
/* first, the help pages: open, page to the end (extra presses are ignored), back one, leave */
#ifdef __GEOS__     /* 11 pages, one press past the last; OPTION twice: dumps of the title drawn again */
static const unsigned char paging[] = { K_HELP, '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<',
                                        K_ESC, K_OPTION, K_OPTION, 0 };
#else
static const unsigned char paging[] = { K_HELP, '>', '>', '>', '>', '>', '>', '>', '>', '<', K_ESC, 0 };
#endif

#ifdef __GEOS__
/* GEOS sends a BRK to BRKVector (its Panic) once atari800's monitor lets it go on: point that at PHP, RTI,
 * which goes back after the BRK with IRQs off, as the program runs (src/geos/screen.s). */
static const unsigned char brk_back[] = { 0x08, 0x40 };
#define BRKVector (*(const void **)0x84AF)
#define DUMP() do { BRKVector = brk_back; __asm__ ("brk"); __asm__ ("nop"); } while (0)
/* The menu, once, part-way into the first game: the pointer's moves and button (x, y, down,
 * dump after), given to display_gui.c as in_key() would. file opened, Log off highlighted,
 * closed by a click elsewhere; then file, Quit to DeskTop, which logs off, quits from the
 * title, and (on run-geos.sh's disk, where it is the DeskTop) starts again. */
unsigned char __fastcall__ gui_poll(unsigned char k, unsigned x, unsigned char y, unsigned char down);
static const unsigned char clicks[][4] = {
    { 15, 5, 1, 0 }, { 15, 5, 0, 0 }, { 60, 18, 0, 1 }, { 200, 150, 1, 0 }, { 200, 150, 0, 1 },
#define QUIT_AT 5                           /* the log's history first: up three, a dump each, down one */
    { 15, 5, 1, 0 }, { 15, 5, 0, 0 }, { 60, 28, 1, 0 }, { 60, 28, 0, 0 },   /* Quit to DeskTop */
};
static unsigned char clicked;
/* The instruction pages mid-game, once, after the history and before Quit, opened while another seat
 * plays: a dump before each key; after the first page they stay up until the game logs something new
 * (or about a minute: 200 key times), so it goes on under them; closed, a dump of the game as it went on
 * meanwhile, the new line in the log (K_DOWN: the log is at its newest already). */
static const unsigned char midhelp[] = { K_HELP, '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
                                         '<', '<', '<', '<', '<', '<', K_ESC, K_DOWN, 0 };
static unsigned char helped, seen[8], waits;
extern const unsigned char *key_queue;      /* src/geos/input.c */
#define SCROLL_NOW (clicked == QUIT_AT)
#elif defined(__CC65__) && !defined(__NES__)
#define DUMP() do { __asm__ ("brk"); __asm__ ("nop"); } while (0)
#else
void pc_dump(void);                         /* test/pc_harness.c, test/nes_dump.c */
#define DUMP() pc_dump()
#endif

#if defined(__GEOS__) || defined(AUTO_SCROLL)
/* The log's history (LOG_HISTORY), once: up three lines, a dump after each, then down one.
 * On by itself on GEOS; elsewhere -DAUTO_SCROLL (not in the captures' builds). */
#ifndef SCROLL_NOW
#define SCROLL_NOW (turn != NONE)                   /* in a game */
#endif
static const unsigned char scroll[] = { K_UP, K_UP, K_UP, K_DOWN, 0 };
static unsigned char dumps, scrolled, dump_next;
#endif

static unsigned char rnd(void)
{
    seed = (seed << 1) ^ ((seed & 0x80) ? 0x1D : 0);   /* 8-bit LFSR */
    return seed;
}

#ifdef AUTOCLICK
unsigned char amiga_autoclick(unsigned char key);   /* test/amiga_autoclick.c: click the widget that gives `key`; 0 if none */
#endif

/* the key a player might press for the open prompt */
static unsigned char pick(unsigned char r)
{
    switch (pr_kind) {
    case 'S':
        return '3';
    case 'P':
        return '1' + (r & 7);                       /* 7 and 8 are ignored */
    case 'F':
    case 'K':
        return syn_letter[r % NSYN];
    case 'D':
#ifdef __AMIGA__
        if (complaint)                              /* refused: clear the count, or wait out the server's minute */
            return K_DELETE;
#endif
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

unsigned char auto_key(void)
{
    unsigned char t = ticks(), k;
#ifdef AUTOCLICK
    if ((k = in_key()) != 0)                        /* the keys a click queued come out one at a time */
        return k;
#endif
    elapsed += (unsigned char)(t - prev);
    prev = t;
    if (!status_len && !status_code && shown == 0xFF)
        return 0;                                   /* the title, until the server answers: its status shows */
    if (DUMP_TICKS && (elapsed >= DUMP_TICKS || pr_kind != shown)) {
        elapsed = 0;
        shown = pr_kind;
        DUMP();
#if defined(__GEOS__) || defined(AUTO_SCROLL)
        ++dumps;
#endif
    }
#ifdef __GEOS__
    if (key_queue)
        return in_key();                            /* a menu's keys (Quit's) go to the game */
#endif
#ifndef AUTOCLICK
    in_key();
#endif
    if ((unsigned char)(t - last) < KEY_TICKS)
        return 0;
    last = t;
#if defined(__GEOS__) || defined(AUTO_SCROLL)
    if (dump_next) {                                /* after a key's effect */
        dump_next = 0;
        DUMP();
    }
    if (dumps > 8 && SCROLL_NOW && scroll[scrolled]) {
        dump_next = 1;
        return scroll[scrolled++];
    }
#endif
#ifdef __GEOS__
    if (dumps > 8 && SCROLL_NOW && midhelp[helped] && (helped || (turn != NONE && !pr_kind))) {
        if (!helped)
            memcpy(seen, log_line(0), sizeof seen);
        else if (helped == 1 && waits < 200 && !memcmp(seen, log_line(0), sizeof seen)) {
            ++waits;
            return 0;
        }
        DUMP();
        return midhelp[helped++];
    }
    if (dumps > 8 && clicked < sizeof clicks / sizeof clicks[0] && (!SCROLL_NOW || !midhelp[helped])) {
        k = gui_poll(0, clicks[clicked][0], clicks[clicked][1], clicks[clicked][2]);
        if (clicks[clicked++][3])
            DUMP();
        return k;
    }
#endif
#if REGION_PICK
    if (regioning[regioned]) {                      /* the region panel first: a dump before each key */
        if (DUMP_TICKS)
            DUMP();
        return regioning[regioned++];
    }
#endif
    if (paging[paged]) {                            /* a dump before each key shows each page */
        if (DUMP_TICKS)
            DUMP();
#ifdef AUTOCLICK
        if (paging[paged] == K_HELP && amiga_autoclick(K_HELP)) {      /* the HELP button */
            ++paged;
            return 0;
        }
#endif
        return paging[paged++];
    }
    if (name[typed]) {                              /* title: the handle, then Return */
#ifdef AUTOCLICK
        if (name[typed] == '\r' && amiga_autoclick('\r')) {   /* Return is the JACK-IN button */
            ++typed;
            return 0;
        }
#endif
        return name[typed++];
    }
    k = pick(rnd());
#ifdef AUTOCLICK
    if (k && amiga_autoclick(k))
        return 0;                                   /* the click queued it (or them) */
#endif
    return k;
}
