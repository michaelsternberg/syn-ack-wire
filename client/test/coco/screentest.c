/* screentest.c -- the CoCo 1/2 proof of concept for SYN-ACK WIRE's screen: 40x24
 * text in 6x8 cells on PMODE 4, drawn by src/coco/text.c from src/coco/font.txt.
 * Nothing here talks to the network: it draws three still screens (the title,
 * a game in progress, a help page) so the font and the colours can be judged,
 * and times the drawing with BASIC's 60 Hz TIMER. The fourth is the game's
 * title logo with its static running (src/coco/snow.c), and the time a pass of
 * the static takes.
 *
 *   SPACE  the next screen         C  green on black / black on green
 *   T      time it again           BREAK  back to BASIC
 *
 * The screen is at $0E00-$25FF, where HDB-DOS leaves room (the lobby puts its
 * own there), and the program loads above it at $2600.
 *
 * Strings use the help pages' marking: text between { and } shows inverse, and
 * the braces take no cell. \\ is the yen sign. Lower case shows as upper case:
 * the client's font has the title's box glyphs there (font_coco.py). */
#include <coco.h>
#include "../../src/coco/text.h"
#include "../../src/coco/title_art.h"

void snow(unsigned char green_bg);      /* src/coco/snow.c */

#define ROWS        TXT_ROWS
#define CYCLES_TICK 14915UL             /* 894886 Hz / 60 */
#define TIME_ROWS   48
#define TIME_SNOW   60                  /* passes of the static timed */
#define SCREENS     4

static const char *title_rows[ROWS] = {
    0,
    0,                                  /* rows 1-2: the timings and the keys */
    0,
    0,
    0,
    "      +-+-+-+-+-+-+-+",
    "      |S|Y|N|-|A|C|K|",
    "------+-+-+-+-+-+-+-+---+-+-+-+-+---o",
    "                        |W|I|R|E|",
    "                        +-+-+-+-+",
    0, 0, 0, 0, 0,
    "    REGION  NEO-KANSAI-1",
    "    STATUS  LOBBY OPEN: 2 WAITING",
    "    HANDLE  ZER0COOL",
    0,
    "    {CLEAR+H}   MOVE HINTS {ON}",
    "    {CLEAR+S}   TOGGLE SOUND",
    "    {?}         INSTRUCTIONS, ANY TIME",
    0,
    " TYPE A HANDLE, THEN ENTER",
};

static const char *game_rows[ROWS] = {
    "{  NEO-KANSAI-1:CONNECTED   SYN-ACK WIRE }",
    "  ABCDEFGHIJKL   HIVES  PRICE LEFT",
    "0 ..........+.   {G}  -",
    "1 .6..{L}.......   {R}  -",
    "2 ...{LLL}......   {D}  -",
    "3 .....{L}4{I}....   {I}  3    \\400   15",
    "4 .......{II}...   {B}  -",
    "5 ............   {L}  5    \\700   19",
    "6 .+2.........   {O}  -",
    "7 .........5..",
    "8 ..........1. LAST: E1",
    0,
    " NETRUNNER    ACCT  {G}  {R}  {D}  {I}  {B}  {L}  {O}",
    ">{ZER0COOL}    \\3200  .  .  .  4  .  2  .",
    " MAMBA       \\4100  .  .  .  4  .  3  .",
    " RAZORGRL    \\2800  .  .  .  2  .  1  .",
    " -",
    " -",
    " -",
    0,
    " [+] ZER0COOL SPIKES E1",
    " [*] LOTUS-12 HIVE SPREADS: 5 NODES",
    "{ BUY UP TO 3 CRYPTO:                    }",
    "{ LETTERS, <-, ENTER TO BUY            60}",
};

static const char *help_rows[ROWS] = {
    "{ HOW TO PLAY                       1/11 }",
    0,
    "Crime syndicates want control of the",
    "region's network and reward netrunners",
    "like you to gain access.",
    0,
    "Compromise a network node using your",
    "exploit kit. Compromise two adjacent",
    "nodes to create a botnet hive. Sell it",
    "to a syndicate for one of their crypto.",
    0,
    "Hives spread and merge. Larger hives",
    "hivejack smaller ones. The smaller",
    "hive's top two crypto backers make bank.",
    0,
    "But you know the real \\\\\\ is dealing in",
    "the crypto market and playing syndicates",
    "against each other. That is, until a",
    "null-route ends the gig.",
    0,
    "Turn pages with < > or the arrow keys.",
    "ESC goes back to the title.",
    0,
    "{                ESC:EXIT         NEXT ->}",
};


static unsigned char green_bg;          /* 1: black on green */
static unsigned char which;             /* the screen showing */
static unsigned full_ticks, row_ticks, snow_ticks;  /* the last timings */

/* A row's cells from s (or nothing), padded with spaces. */
static void set_row(unsigned char row, const char *s)
{
    unsigned char *c = txt_cells + (unsigned)row * TXT_COLS;
    unsigned char col = 0, inv = 0;

    if (s)
        for (; *s && col < TXT_COLS; ++s) {
            if (*s == '{')
                inv = TXT_INV;
            else if (*s == '}')
                inv = 0;
            else
                c[col++] = (unsigned char)((*s >= 'a' && *s <= 'z' ? *s - 32 : *s) | inv);
        }
    for (; col < TXT_COLS; ++col)
        c[col] = ' ';
}

static char *number(char *d, unsigned v)
{
    char buf[6];
    unsigned char n = 0;

    do {
        buf[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *d++ = buf[--n];
    return d;
}

static char *copy(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    return d;
}

/* The screen showing, into the cells; the title gets the timings on row 1 and
 * the keys on row 2. */
static void fill(void)
{
    const char **rows = which == 0 ? title_rows : which == 1 ? game_rows : help_rows;
    const unsigned char *p;
    char line[48];
    char *d = line;
    unsigned char r;

    if (which == 3) {                   /* the logo, and the static's time on row 16 */
        for (r = 0; r < ROWS; ++r)
            set_row(r, 0);
        for (p = title_art; *p != 0xFF; p += 3 + p[2])
            memcpy(txt_cells + (unsigned)p[0] * TXT_COLS + p[1], p + 3, p[2]);
        d = copy(d, " {STATIC} ");
        d = number(d, (unsigned)((unsigned long)snow_ticks * CYCLES_TICK / TIME_SNOW));
        d = copy(d, " CYCLES A PASS");
        *d = 0;
        set_row(16, line);
        set_row(18, " SPACE NEXT, C COLOURS, BREAK");
        return;
    }
    for (r = 0; r < ROWS; ++r)
        set_row(r, rows[r]);
    if (which)
        return;
    d = copy(d, " {FULL} ");
    d = number(d, full_ticks);
    d = copy(d, "/60 S  {A ROW} ");
    d = number(d, (unsigned)((unsigned long)row_ticks * CYCLES_TICK / TIME_ROWS));
    d = copy(d, " CYCLES");
    *d = 0;
    set_row(1, line);
    set_row(2, " SPACE NEXT, C COLOURS, T TIME, BREAK");
}

/* The whole screen, then TIME_ROWS single rows (each with the line above). */
static void time_it(void)
{
    unsigned t;
    unsigned char i;

    which = 0;
    fill();
    t = getTimer();
    txt_all();
    full_ticks = getTimer() - t;
    t = getTimer();
    for (i = 0; i < TIME_ROWS; ++i)
        txt_row((unsigned char)(i & 15));
    row_ticks = getTimer() - t;
    t = getTimer();
    for (i = 0; i < TIME_SNOW; ++i)
        snow(green_bg);
    snow_ticks = getTimer() - t;
    fill();
    txt_all();
}

int main(void)
{
    unsigned char k;

    initCoCoSupport();
    txt_init();
    txt_show();
    time_it();
    for (;;) {
        if (which == 3)
            snow(green_bg);
        k = inkey();
        if (k == ' ') {                 /* row by row, over the last screen: txt_row's test */
            which = (unsigned char)((which + 1) % SCREENS);
            fill();
            for (k = 0; k < ROWS; ++k)
                txt_row(k);
        } else if (k == 'C') {
            green_bg = !green_bg;
            txt_colours(green_bg);
        } else if (k == 'T') {
            time_it();
        } else if (k == 3) {            /* BREAK */
            break;
        }
    }
    screen(0, 0);
    return 0;
}
