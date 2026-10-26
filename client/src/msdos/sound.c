/* Chords for board events: on an AdLib (or a Sound Blaster's FM part) when one
 * answers at 388h, else on the PC speaker.
 *
 * AdLib: the Atari's chords (src/atari/sound.s), three voices at once with an
 * instrument that dies away by itself. Nothing blocks: snd_poll(), called with
 * every key check (input.c), lets a chord go when its time is up and starts the
 * one that follows it.
 *
 * Speaker: the Apple II's (src/apple2/sound.s), the same pitches and lengths.
 * The speaker plays one tone at a time, so a chord is up to three short notes in
 * a row; the PIT's channel 2 makes each tone, and a delay loop, timed against
 * the BIOS tick at start, holds it. It blocks while it plays (well under half a
 * second); FujiNet holds the bytes that arrive meanwhile, as on the Apple. */
#include <conio.h>
#include "../platform.h"

#define BIOS_TICK (*(volatile unsigned char __far *)0x0040006CL)

#define NOTES   3                       /* a chord's notes; 0 ends it early */
#define GAP_MS  8                       /* quiet after each note, so two alike stay two */

/* per chord (platform.h order), NOTES of {PIT divisor (1193182 / Hz), milliseconds} */
static const unsigned short chords[7][NOTES][2] = {
    { { 2601,  44 } },                              /* SPIKE: 459 Hz, a short low click */
    { { 1785,  52 }, { 1435,  54 } },               /* BUY: 669, 832 Hz, two rising */
    { { 1435,  60 }, { 1085,  64 } },               /* HIVE: 832, 1100 Hz */
    { { 1318,  50 }, { 1901,  56 } },               /* JACK: 905, 628 Hz, falling */
    { { 2951,  62 }, { 2951,  62 } },               /* ICE: 404 Hz, two low knocks */
    { { 1668,  63 }, { 1201,  60 } },               /* TURN: 715, 993 Hz, two rising */
    { { 1901,  96 }, { 1551,  98 }, { 1201, 111 } } /* END: 628, 769, 993 Hz, a longer series */
};

static unsigned char on, adlib;
static unsigned long per_ms;            /* delay loop passes in a millisecond */

/* ---------------------------------------------------------------- AdLib
 * Notes are block << 10 | F-number (f = F-number * 49716 / 2^(20 - block)),
 * within 1 Hz. The chords are the Atari's: entries 0-6 are platform.h's, the
 * rest only follow on; a length in BIOS ticks, from the Atari's volume steps. */
#define OPL     0x388
#define D3 0x0B06
#define E3 0x0B65
#define F3 0x0B99
#define G3 0x0E05
#define A3 0x0E44
#define B3 0x0E8B
#define C4 0x0EB2
#define D4 0x0F06
#define E4 0x0F65
#define F4 0x0F99
#define G4 0x1205
#define A4 0x1244
#define NONE 0xFF

static const unsigned short fm_chord[11][3] = {
    { D3, F3, A3 }, { C4, E4, G4 }, { C4, E4, G4 }, { B3, D4, F4 },   /* spike, buy, hive, hivejack */
    { E3, G3, B3 }, { G3, B3, D4 }, { F3, A3, C4 },                   /* Black ICE, turn, null-route */
    { C4, F4, A4 }, { C4, E4, G4 }, { C4, E4, G4 }, { B3, F4, A4 }    /* what follows buy, turn, null-route */
};
static const unsigned char fm_ticks[11] = { 4, 5, 12, 12, 18, 5, 9, 5, 5, 9, 46 };
static const unsigned char fm_next[11] = { NONE, 7, NONE, NONE, NONE, 8, 9, NONE, NONE, 10, NONE };

static unsigned char sounding = NONE, prio, began;

static void opl(unsigned char reg, unsigned char val)
{
    unsigned char k;
    outp(OPL, reg);
    for (k = 0; k < 6; ++k)             /* 3.3 us after the address */
        inp(OPL);
    outp(OPL + 1, val);
    for (k = 0; k < 35; ++k)            /* 23 us after the data */
        inp(OPL);
}

/* The usual test: timer 1 runs out and sets the status flags only on a real OPL2. */
static unsigned char opl_found(void)
{
    unsigned char s1, s2, k;
    opl(0x04, 0x60);                    /* reset both timers */
    opl(0x04, 0x80);                    /* and the IRQ */
    s1 = inp(OPL);
    opl(0x02, 0xFF);                    /* timer 1: 80 us to run out */
    opl(0x04, 0x21);                    /* start it */
    for (k = 0; k < 200; ++k)           /* well over 80 us, even on a fast machine */
        inp(OPL);
    s2 = inp(OPL);
    opl(0x04, 0x60);
    opl(0x04, 0x80);
    return (s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0;
}

/* Voices 0-2 (operators 0/3, 1/4, 2/5): a soft struck sound that fades on its own */
static void opl_setup(void)
{
    static const unsigned char op[3] = { 0, 1, 2 };
    unsigned char v;
    for (v = 0; v < 0xF6; ++v)          /* every register to 0 */
        opl(v, 0);
    opl(0x01, 0x20);                    /* waveform select on */
    for (v = 0; v < 3; ++v) {
        opl(0x20 + op[v], 0x01);        /* modulator: multiple 1, percussive envelope */
        opl(0x40 + op[v], 0x1A);        /*   output level: a little brightness */
        opl(0x60 + op[v], 0xF4);        /*   attack 15, decay 4 */
        opl(0x80 + op[v], 0x56);        /*   sustain 5, release 6 */
        opl(0x23 + op[v], 0x01);        /* carrier: multiple 1, percussive envelope */
        opl(0x43 + op[v], 0x04);        /*   output level: loud */
        opl(0x63 + op[v], 0xF3);        /*   attack 15, decay 3 */
        opl(0x83 + op[v], 0x58);        /*   sustain 5, release 8 */
        opl(0xC0 + v, 0x06);            /* feedback 3, FM */
    }
}

static void fm_off(void)
{
    unsigned char v;
    if (sounding == NONE)
        return;
    for (v = 0; v < 3; ++v)             /* key off, keeping the pitch: it releases */
        opl(0xB0 + v, fm_chord[sounding][v] >> 8);
    sounding = NONE;
}

static void fm_start(unsigned char c)
{
    unsigned char v;
    fm_off();
    for (v = 0; v < 3; ++v) {
        opl(0xA0 + v, fm_chord[c][v] & 0xFF);
        opl(0xB0 + v, 0x20 | fm_chord[c][v] >> 8);      /* key on */
    }
    sounding = c;
    began = BIOS_TICK;
}

/* With every key check: let a chord go when its time is up; start what follows it. */
void snd_poll(void)
{
    unsigned char c = sounding;
    if (c == NONE || (unsigned char)(BIOS_TICK - began) < fm_ticks[c])
        return;
    fm_off();
    if (fm_next[c] != NONE)
        fm_start(fm_next[c]);
}

/* ---------------------------------------------------------------- speaker */

static void spin(unsigned long passes)
{
    volatile unsigned long k = passes;
    while (k)
        --k;
}

static void wait_ms(unsigned short ms)
{
    while (ms--)
        spin(per_ms);
}

/* Time the delay loop: its passes in one BIOS tick (54.9 ms), counted in runs of
 * CHUNK so the call around each run costs next to nothing. */
#define CHUNK 100

void snd_init(void)
{
    unsigned char t;
    unsigned long passes = 0;
    on = 1;
    t = BIOS_TICK;
    while (BIOS_TICK == t)              /* start on a tick's edge */
        ;
    t = BIOS_TICK;
    while (BIOS_TICK == t) {
        spin(CHUNK);
        passes += CHUNK;
    }
    per_ms = passes * 10 / 549;
    if (!per_ms)
        per_ms = 1;
    adlib = opl_found();
    if (adlib)
        opl_setup();
}

static void tone(unsigned short divisor)
{
    outp(0x43, 0xB6);                   /* channel 2, both bytes, square wave */
    outp(0x42, divisor & 0xFF);
    outp(0x42, divisor >> 8);
    outp(0x61, inp(0x61) | 3);          /* the gate and the speaker on */
}

static void quiet(void)
{
    outp(0x61, inp(0x61) & ~3);
}

void play_chord(unsigned char n)
{
    unsigned char k;
    if (!on)
        return;
    if (adlib) {                        /* a sounding chord gives way only to a higher one */
        if (sounding != NONE && n < prio)
            return;
        prio = n;
        fm_start(n);
        return;
    }
    for (k = 0; k < NOTES && chords[n][k][0]; ++k) {
        tone(chords[n][k][0]);
        wait_ms(chords[n][k][1]);
        quiet();
        wait_ms(GAP_MS);
    }
}

/* Silence, before the program ends: an AdLib chord released, the speaker off. */
void snd_off(void)
{
    if (adlib)
        fm_off();
    quiet();
}

void snd_toggle(void)
{
    on ^= 1;
    if (on)
        play_chord(CHORD_HIVE);
    else if (adlib)
        fm_off();
}
