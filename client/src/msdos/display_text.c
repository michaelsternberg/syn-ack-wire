/* The text-only display for MS-DOS: the 40x24 layout in the middle of the
 * 80x25 text screen (columns 20-59), or filling a 40x25 one, written straight
 * to video memory. 40 columns are for a TV or a composite monitor, where 80
 * blur: the player picks (setup), and F4 on the title screen swaps. On a colour
 * adapter F5 turns green phosphor on or off, anywhere (below); it starts off, as
 * colour can be hard to read on a grey LCD panel. The
 * layout and the code are the Apple II's (src/apple2/display_text.c). Only
 * monochrome attributes, so it reads the same on MDA, Hercules, CGA, EGA, VGA
 * and the grey LCD panels of old laptops:
 *
 *  normal   the Grid's empty nodes (.), lone spikes (+) and move hints (1-6),
 *           the panel, the seats, the log; rows 22-23 while others play
 *  reverse  the title bar, hives (letters), Black ICE sizes, the syndicate
 *           letters, whose turn it is; rows 22-23 while this seat has a
 *           decision to make (inside them, what normally shows reverse shows
 *           normal); a refusal at the foot of the title screen
 *  blink    the last move, reverse and blinking, for FLASH_SECS; then it shows
 *           as usual
 *
 * A cell is two bytes, character then attribute. Text arrives as ASCII; "\"
 * from the server (the Atari's yen sign) shows as code page 437's yen.
 *
 * In 80 columns, the 20 either side of the game hold circuit-board art (margins.txt):
 * decoration only, drawn with every full redraw: normal, as dark grey doesn't
 * show on a monochrome adapter or a grey LCD panel; green in colour. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"

#ifdef PC_SCREEN                        /* a PC build for the manual's captures (test/pc_harness.c) */
#define FAR
#define FMEMCPY memcpy
extern unsigned short PC_SCREEN[25 * 80];
#else
#include <i86.h>
#include <conio.h>                      /* inp, outp */
#include <stdlib.h>                     /* _psp */
#define FAR __far
#define FMEMCPY _fmemcpy                /* video memory is in another segment */
#endif
typedef unsigned short FAR cell;

#define NORMAL  0x07                    /* attribute: light grey on black */
#define INV     0x70                    /* black on light grey */
#define FLASH   0xF0                    /* the same, blinking */
#define HIVE    0x7F                    /* a hive's letter: reverse, or its syndicate's colours */
#define FLIP    (NORMAL ^ INV)          /* mode ^ FLIP: normal <-> reverse */
#define YEN     0x9D                    /* code page 437 */


#define FLASH_SECS  5

#define ROW_BOARD   2
#define COL_BOARD   2
#define COL_PANEL   15                  /* 25 columns: HIVES letter and size, gap, name (15) */
#define COL_NAME    24                  /* or, while buying, price and crypto left */
#define ROW_SEATS   13
#define ROW_LOG     19
#define LOG_ROWS    3
#define ROW_INFO    22
#define ROW_ASK     23

/* The title's lines sit low, as on the Atari, whatever the logo's height */
#define ROW_REGION  15                  /* then STATUS and HANDLE */
#define ROW_KEYS    18                  /* F2, F3, F1, then F4 and F5 on a colour adapter */
#define COL_KEYS    18                  /* what each key does */
#define COL_HINTS   (COL_KEYS + 11)     /* ON/OFF after "MOVE HINTS", 40/80 after "COLUMNS" */

#include "title_art.h"                  /* title_art[], TITLE_END */
#include "margins.h"                    /* margins[], MARGIN_W */
#if TITLE_END >= ROW_REGION - 1
#error "the title logo runs into the title's lines: at most rows 0-13"
#endif

static cell *screen;                    /* row 0, column 0 of the screen */
static cell *cur;
static unsigned char cols = 80, left = 20;  /* the screen's width; where the game's 40 columns start */
static unsigned char mono;              /* a monochrome adapter: 80 columns only, no colour */
static unsigned char colour;            /* green phosphor (F5), on a colour adapter */
static unsigned char cr, cc;            /* the row and column put() writes next */
static unsigned char helping;           /* the instructions are up */
static unsigned char mode = NORMAL, ready, i, j, n, v;
#ifndef PC_SCREEN
static unsigned char old_mode;          /* the BIOS video mode the program found, put back on the way out */
#endif
static unsigned char field;             /* the playfield is up: the title bar may draw */
static unsigned char bottom = NORMAL;   /* rows 22-23: INV while this seat decides */

#define ROW(r)  (screen + (r) * cols + left)

/* ---------------------------------------------------------------- drawing */

static void at(unsigned char r, unsigned char c);
static void put(unsigned char ch);
static void text(const char *s);
static void clear_screen(void);

#ifndef PC_SCREEN
#define BIOS_CGA_MODE (*(unsigned char __far *)0x00400065L)  /* the BIOS's copy of port 3D8 */
#define MODEL_BYTE    (*(unsigned char __far *)0xF000FFFEL)  /* FDh: a PCjr */

/* The BIOS text mode for `cols` and `colour`: 1 (40x25) or 3 (80x25) in colour,
 * else 0 or 2, the same with a composite monitor's colour burst off, which
 * makes text sharper there; the cursor hidden, as the game draws its own. In
 * colour the hardware blink goes off, so backgrounds can be bright: by the BIOS
 * on an EGA or VGA (INT 10h AX=1003h), the video gate array's mode control
 * register 2 on a PCjr, port 3D8 on a CGA or a Tandy 1000. */
static void set_width(void)
{
    union REGS r;
    left = cols == 80 ? 20 : 0;
    r.x.ax = (cols == 80 ? 2 : 0) + colour;
    int86(0x10, &r, &r);
    r.h.ah = 0x01;                      /* cursor shape: off */
    r.x.cx = 0x2000;
    int86(0x10, &r, &r);
    if (!colour)
        return;
    r.x.ax = 0x1003;
    r.h.bl = 0;
    int86(0x10, &r, &r);
    if (MODEL_BYTE == 0xFD) {
        inp(0x3DA);                     /* the gate array's next write is a register number */
        outp(0x3DA, 3);
        outp(0x3DA, 0);
    } else {
        BIOS_CGA_MODE &= ~0x20;
        outp(0x3D8, BIOS_CGA_MODE);
    }
}

/* The player picks the width, asked in 40 columns, which every screen shows clearly. */
static void ask_width(void)
{
    cols = 40;
    set_width();
    clear_screen();
    at(8, 20 - (sizeof TH_TITLE - 1) / 2);
    text(TH_TITLE);
    at(11, 5);
    text("SCREEN WIDTH:");
    at(13, 5);
    text("1) 40 COLUMNS (TV, COMPOSITE)");
    at(14, 5);
    text("2) 80 COLUMNS");
    at(19, 4);
    text("SET SYNACK=40 OR 80 TO SKIP THIS");
    while ((v = in_key()) != '1' && v != '2')
        ;
    cols = v == '1' ? 40 : 80;
}

/* The first letter of SYNACK= in DOS's environment (the segment at offset 2Ch
 * of the program's PSP: NAME=value strings, an empty one last), or 0. Read
 * there rather than with getenv, which copies the whole environment first. */
static unsigned char env_width(void)
{
    static const char name[] = "SYNACK=";
    const char __far *e = MK_FP(*(unsigned short __far *)MK_FP(_psp, 0x2C), 0);
    while (*e) {
        for (n = 0; name[n] && e[n] == name[n]; ++n)
            ;
        if (!name[n])
            return e[n];
        while (*e++)
            ;
    }
    return 0;
}
#endif

/* Text mode, once. A monochrome adapter (BIOS mode 7) stays as it is, at B000,
 * in 80 columns. Anything else is at B800, in 40 or 80 columns: SYNACK=40 or
 * SYNACK=80 in the environment says which, else the player picks. */
static void setup(void)
{
#ifndef PC_SCREEN
    union REGS r;
#endif
    if (ready)
        return;
    ready = 1;
#ifdef PC_SCREEN
    screen = PC_SCREEN;
#else
    r.h.ah = 0x0F;                      /* the current video mode */
    int86(0x10, &r, &r);
    old_mode = r.h.al;
    if (r.h.al == 7) {
        screen = (cell *)0xB0000000L;
        mono = 1;
        r.h.ah = 0x01;                  /* cursor shape: off */
        r.x.cx = 0x2000;
        int86(0x10, &r, &r);
        return;
    }
    screen = (cell *)0xB8000000L;
    v = env_width();
    if (v == '4' || v == '8')
        cols = v == '4' ? 40 : 80;
    else
        ask_width();
    set_width();
#endif
}

/* F5, anywhere but the instructions: colour on or off, and the screen drawn
 * again. The playfield's log isn't in the game state, so its lines are kept. */
void display_colour(void)
{
#ifndef PC_SCREEN
    static unsigned char log[LOG_ROWS][40];
    if (mono || helping || !ready)
        return;
    for (i = 0; i < LOG_ROWS; ++i)     /* before the mode change clears the screen */
        for (j = 0; j < 40; ++j)
            log[i][j] = (unsigned char)ROW(ROW_LOG + i)[j];
    colour ^= 1;
    set_width();
    if (!field) {
        display_title_screen();
        return;
    }
    display_playfield();
    for (i = 0; i < LOG_ROWS; ++i) {
        at(ROW_LOG + i, 0);
        for (j = 0; j < 40; ++j)
            put(log[i][j]);
    }
#endif
}

/* On the way out to DOS: the video mode the program found, which also brings
 * back the cursor, the screen cleared, and the hardware blink. */
void display_restore(void)
{
#ifndef PC_SCREEN
    union REGS r;
    if (!ready)
        return;
    r.h.ah = 0x00;
    r.h.al = old_mode;
    int86(0x10, &r, &r);
#endif
}

/* F4 on the title screen: the other width, and the title drawn again in it. */
void display_width(void)
{
#ifndef PC_SCREEN
    if (field || mono || helping || !ready)
        return;
    cols ^= 40 ^ 80;
    set_width();
    display_title_screen();
#endif
}

/* ---------------------------------------------------------------- colour
 * The drawing code writes NORMAL, INV, FLASH and HIVE, the monochrome
 * attributes (HIVE shows as INV); in colour they become green phosphor by
 * where the cell is: light green on black, black on green under the Grid's
 * nodes and the seats' rows (not the labels or headers), as on the Atari; reverse black on light green, the
 * title bar white on green, the rows of a question for this seat white on
 * magenta. Each syndicate's letter has its own background with a bright letter.
 * Bright backgrounds need the hardware blink off (set_blink), so in colour the
 * last move blinks in software (display_idle). */
#define C_TEXT      0x0A                /* light green on black */
#define C_PANEL     0x20                /* black on green */
#define C_INV       0xA0                /* black on light green */
#define C_BAR       0x2F                /* white on green */
#define C_ASK       0x5F                /* white on magenta */
#define C_LAST      0xF0                /* black on white: the last move, lit */
#define C_ART       0x02                /* the side panels: green on black */
/* the syndicates in theme order, never green or magenta: cyan, red, brown, light grey, blue, yellow, dark grey */
static const unsigned char hive_colour[NSYN] = { 0x3F, 0x4F, 0x6E, 0x70, 0x1F, 0xE0, 0x8F };

static unsigned char attr(unsigned char m, unsigned char ch, unsigned char r, unsigned char c)
{
    if (!colour)
        return m == HIVE ? INV : m;
    if (m == HIVE) {
        unsigned char k;
        for (k = 0; k < NSYN - 1 && syn_letter[k] != ch; ++k)
            ;
        return hive_colour[k];
    }
    if (m == FLASH)
        return C_LAST;
    if (m == INV)
        return r == 0 ? C_BAR : r >= ROW_INFO ? C_ASK : C_INV;
    return field && ((r >= ROW_BOARD && r < ROW_BOARD + RULE_ROWS && c >= COL_BOARD && c < COL_BOARD + RULE_COLS)
                     || (r >= ROW_SEATS && r < ROW_SEATS + SEATS)) ? C_PANEL : C_TEXT;
}

static void at(unsigned char r, unsigned char c)
{
    cur = ROW(r) + c;
    cr = r;
    cc = c;
}

static void put(unsigned char ch)
{
    if (ch == '\\')
        ch = YEN;
    *cur++ = attr(mode, ch, cr, cc++) << 8 | ch;
}

static void text(const char *s)
{
    while (*s)
        put(*s++);
}

static void textn(const unsigned char *s, unsigned char len)
{
    while (len--)
        put(*s++);
}

static void spaces(unsigned char k)
{
    while (k--)
        put(' ');
}

static void clear_row(unsigned char r)
{
    at(r, 0);
    spaces(40);
}

static void clear_screen(void)
{
    unsigned k;
    unsigned char a = colour ? C_ART : NORMAL;
    mode = NORMAL;
    if (cols == 80) {
        for (k = 0; k < 25 * 80; ++k)
            screen[k] = (colour ? C_TEXT : NORMAL) << 8 | ' ';
        for (k = 0; k < 25; ++k)        /* the side panels */
            for (j = 0; j < MARGIN_W; ++j) {
                screen[k * 80 + j] = a << 8 | margins[k][j];
                screen[k * 80 + 80 - MARGIN_W + j] = a << 8 | margins[k][MARGIN_W + j];
            }
    }
    for (k = 0; k < 25; ++k) {          /* the game's 40 columns, cell by cell for the panels' colour */
        at(k, 0);
        spaces(40);
    }
}

/* Right-aligned decimal in `width` columns. `money` (units of 100) appends "00"
 * and puts the currency sign right before the digits, inside `width`. */
static unsigned char digits[7];

static void number(unsigned value, unsigned char width, unsigned char money)
{
    static const unsigned pow10[5] = { 10000, 1000, 100, 10, 1 };
    unsigned char len = 0, d, k;
    for (k = 0; k < 5; ++k) {
        for (d = '0'; value >= pow10[k]; ++d)
            value -= pow10[k];
        if (len || d != '0' || k == 4)
            digits[len++] = d;
    }
    if (money) {
        if (len > 1 || digits[0] != '0') {
            digits[len++] = '0';
            digits[len++] = '0';
        }
        ++len;                          /* the sign */
    }
    if (width > len)
        spaces(width - len);
    if (money) {
        put(YEN);
        --len;
    }
    textn(digits, len);
}

static void node_name(unsigned char node)
{
    for (n = 0; node >= RULE_COLS; ++n)
        node -= RULE_COLS;
    put('A' + node);
    put('0' + n);
}

/* The syndicate letters in `mask`, each flipped from the row's mode. */
static void syn_letters(unsigned char mask)
{
    for (i = 0; i < NSYN; ++i)
        if (mask >> i & 1) {
            v = mode;
            mode = colour ? HIVE : mode ^ FLIP;
            put(syn_letter[i]);
            mode = v;
            put(' ');
        }
}

#if !FANCY_TITLE
/* ---------------------------------------------------------------- title */

void display_title_screen(void)
{
    const unsigned char *p;
    setup();
    field = 0;
    clear_screen();
    for (p = title_art; *p != 0xFF; p += 3 + p[2]) {
        at(p[0], p[1]);
        textn(p + 3, p[2]);
    }
    at(ROW_REGION, 4);
    text("REGION");                     /* from the server, once it answers */
    at(ROW_REGION + 1, 4);
    text("STATUS");                     /* what joining would do, from the server */
    at(ROW_REGION + 2, 4);
    text("HANDLE");
    mode = INV;
    at(ROW_KEYS, 4);
    text("F2");                         /* the key flips it */
    at(ROW_KEYS + 1, 4);
    text("F3");
    at(ROW_KEYS + 2, 4);
    text("F1 ?");
    if (!mono) {
        at(ROW_KEYS + 3, 4);
        text("F4");
        at(ROW_KEYS + 4, 4);
        text("F5");
        at(ROW_KEYS + 3, COL_HINTS);
        put('0' + cols / 10);
        put('0');
        at(ROW_KEYS + 4, COL_HINTS);
        text(colour ? "ON" : "OFF");
    }
    mode = NORMAL;
    at(ROW_KEYS, COL_KEYS);
    text("MOVE HINTS");
    at(ROW_KEYS + 1, COL_KEYS);
    text("TOGGLE SOUND");
    at(ROW_KEYS + 2, COL_KEYS);
    text("INSTRUCTIONS, ANY TIME");
    if (!mono) {
        at(ROW_KEYS + 3, COL_KEYS);
        text("COLUMNS");
        at(ROW_KEYS + 4, COL_KEYS);
        text("COLOR");
    }
    if (refused)
        mode = INV;                     /* a refusal stands out at the foot */
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN"
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN ENTER. ESC QUITS");
    mode = NORMAL;
    display_title_update();
}

/* Each field is padded to its widest, so drawing over the last one leaves nothing behind. */
void display_title_update(void)
{
    at(ROW_REGION, 12);
    textn(region, region_len);
    spaces(sizeof region - region_len);
    at(ROW_REGION + 1, 12);
    n = 0;
    if (status_len) {
        n = status_len - 1;
        textn(status_text, n);
    } else if (status_code == NO_CARRIER) {
        n = 10;
        text("NO CARRIER");
    }
    spaces(sizeof status - 2 - n);      /* 28: to the end of the row */
    at(ROW_REGION + 2, 12);
    textn(edit, edit_len);
    mode = INV;
    put(' ');                           /* cursor */
    mode = NORMAL;
    spaces(8 - edit_len);
    at(ROW_KEYS, COL_HINTS);
    mode = INV;
    text(hints ? "ON" : "OFF");
    mode = NORMAL;
    put(' ');
}
#endif

#if !FANCY_PLAYFIELD
/* ---------------------------------------------------------------- playfield */

/* The last move blinks from when it is first drawn until FLASH_SECS have
 * passed (display_idle); then `done` holds it, and it shows as usual. */
static unsigned char blink = NONE, blink_t, done = NONE, lit;   /* lit: in colour, its software blink's phase */

/* The key ('1'-'6') of the exploit in my kit that can run on `node` now, or 0. */
static unsigned char target(unsigned char node)
{
    if (!hints)
        return 0;
    for (n = 0; n < RULE_KITSIZE; ++n)
        if (kit[n] == node)             /* bit 7 set = can't run: never equal */
            return '1' + n;
    return 0;
}

void display_node(unsigned char node)
{
    unsigned char r, c = node, m = NORMAL;
    for (r = 0; c >= RULE_COLS; ++r)
        c -= RULE_COLS;
    v = board[node];
    if (v == CELL_EMPTY) {
        v = target(node);
        if (!v)
            v = GLYPH_EMPTY;
    }
    else if (v == CELL_SPIKE)
        v = GLYPH_SPIKE;
    else {
        v = syn_letter[v - 2];
        m = HIVE;
    }
    if (node == last_node) {
        at(ROW_BOARD + 7 + 1, COL_PANEL);
        text("LAST: ");
        node_name(node);
        if (node != done) {
            if (blink != node) {
                blink = node;
                blink_t = ticks();
                lit = 0;
            }
            if (!colour || !lit)
                m = FLASH;
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    ROW(ROW_BOARD + r)[COL_BOARD + c] = attr(m, v, ROW_BOARD + r, COL_BOARD + c) << 8 | v;
}

void display_idle(void)
{
    if (blink == NONE)
        return;
    if (blink != last_node)
        blink = NONE;                   /* a newer move took over, or a new game */
    else if ((unsigned char)(v = ticks() - blink_t) >= FLASH_SECS * 18) {
        done = blink;
        blink = NONE;
        display_node(done);
    } else if (colour && (v >> 3 & 1) != lit) {    /* lit and unlit by turns, about twice a second */
        lit ^= 1;
        display_node(blink);
    }
}

void display_hives(void)
{
    at(1, COL_PANEL);
    spaces(2);                          /* over the letters and their sizes */
    text(T_HIVE_P);
    spaces(COL_NAME - COL_PANEL - 2 - (sizeof T_HIVE_P - 1));
    text(pr_kind == 'B' ? "PRICE LEFT" : "SYNDICATES");
    spaces(40 - COL_NAME - 10);
    for (j = 0; j < NSYN; ++j) {
        at(ROW_BOARD + j, COL_PANEL);
        spaces(2);                      /* the letter sits under the start of the header */
        mode = HIVE;
        put(syn_letter[j]);
        mode = NORMAL;
        n = hive_size[j];               /* then its size, next to it */
        if (n) {
            mode = n >= RULE_SAFESIZE ? INV : NORMAL;   /* Black ICE */
            number(n, 3, 0);
            mode = NORMAL;
        } else {
            text("  -");
        }
        spaces(COL_NAME - COL_PANEL - 6);
        if (pr_kind == 'B') {           /* pr: timeout, cash[2], flags, price[7], supply[7] */
            n = pr[4 + j];
            if (n) {
                number(n, 5, 1);
                number(pr[11 + j], 5, 0);
                spaces(40 - COL_NAME - 10);
            } else {
                spaces(40 - COL_NAME);
            }
            continue;
        }
        n = strlen(syn_name[j]);
        text(syn_name[j]);
        spaces(40 - COL_NAME - n);
    }
}

void display_seat(unsigned char seat)
{
    if (seat >= SEATS)
        return;
    at(ROW_SEATS + seat, 0);
    put(seat == me ? '>' : ' ');
    n = seat_kind[seat];
    if (n == SEAT_EMPTY) {
        text("-");
        spaces(38);
        return;
    }
    mode = seat == turn ? INV : NORMAL;
    textn((const unsigned char *)seat_name[seat], 8);
    mode = NORMAL;
    put(n == SEAT_AWAY ? '?' : n == SEAT_BOT ? '*' : ' ');
    number(cash[seat], 8, 1);
    for (j = 0; j < NSYN; ++j) {
        n = coins[seat][j];
        if (n)
            number(n, 3, 0);
        else
            text("  .");
    }
    put(' ');
}

void display_log(void)
{
    for (i = 0; i < LOG_ROWS - 1; ++i)
        for (j = 0; j < 40; ++j)
            ROW(ROW_LOG + i)[j] = ROW(ROW_LOG + i + 1)[j];
    clear_row(ROW_LOG + LOG_ROWS - 1);
    at(ROW_LOG + LOG_ROWS - 1, 1);
    n = msg[1];
    if (n > 39)
        n = 39;
    textn(msg + 2, n);
}

static void kit_line(void)
{
    clear_row(ROW_INFO);
    at(ROW_INFO, 1);
    text(T_EXPLOIT_P);
    for (j = 0; j < RULE_KITSIZE; ++j) {
        v = kit[j];
        put(' ');
        put('1' + j);
        put(':');
        if (v == NONE) {
            text("--");
        } else {
            if (v & 0x80)               /* can't run now: flipped from the row */
                mode ^= FLIP;
            node_name(v & 0x7F);
            mode = bottom;
        }
    }
}

static void ask(const char *s)
{
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(complaint ? complaint : s);
}

/* Row 22 in the bottom rows' mode, when it is kept rather than redrawn. */
static void recolor_info(void)
{
    cur = ROW(ROW_INFO);
    for (i = 0; i < 40; ++i, ++cur)
        *cur = attr(bottom, (unsigned char)*cur, ROW_INFO, i) << 8 | (*cur & 0xFF);
}

void display_prompt(void)
{
    v = pr_kind || turn == me || leaving ? INV : NORMAL;
    if (v != bottom) {
        bottom = v;
        recolor_info();
    }
    mode = bottom;
    if (leaving) {                      /* Esc: the question waits; the prompt comes back after */
        ask(T_LOGOFF " NOW? Y/N");
        mode = NORMAL;
        return;
    }
    if (pr_kind == 'P' || (pr_kind == 0 && turn != NONE))
        kit_line();
    else if (pr_kind != 'S' && pr_kind != 0)
        clear_row(ROW_INFO);            /* in the lobby row 22 keeps the last result */
    at(ROW_INFO, 1);
    switch (pr_kind) {
    case 0:
        clear_row(ROW_ASK);
        if (turn < SEATS && turn != me) {
            at(ROW_ASK, 1);
            text("WAITING FOR ");
            textn((const unsigned char *)seat_name[turn], 8);
            at(ROW_ASK, 32);            /* the instructions, while there's time */
            text("F1 HELP");
        }
        mode = NORMAL;
        return;
    case 'S':
        ask("START WITH HOW MANY SEATS? 2-6");
        break;
    case 'P':
        ask("RUN WHICH " T_EXPLOIT "? PRESS 1-6");
        break;
    case 'F':                           /* pr[1] = available */
        text(T_INFILTRATION "! " T_BROKER " TO ");
        syn_letters(pr[1]);
        ask("PRESS A SYNDICATE LETTER");
        break;
    case 'K':                           /* pr[1] = ctx, pr[2] = options */
        text(pr[1] ? "TIE! ABSORB WHICH NEXT? " : "TIE! WHICH SURVIVES? ");
        syn_letters(pr[2]);
        ask("PRESS A SYNDICATE LETTER");
        break;
    case 'D':                           /* pr: absorbed, absorber, held, supply */
        text(T_HIVEJACK ": ");
        put(syn_letter[pr[2]]);
        text(" TAKES ");
        put(syn_letter[pr[1]]);
        text(". YOU HOLD ");
        number(pr[3], 1, 0);
        put(' ');
        put(syn_letter[pr[1]]);
        ask(step ? T_SWAP " 2 FOR 1, HOW MANY? " : T_CASHOUT " HOW MANY? ");
        if (!complaint)
            textn(edit, edit_len);
        break;
    case 'B':                           /* pr: cash[2], flags, price[7], supply[7] */
        if (step && !edit_len)          /* nothing bought: just the question */
            break;
        text("BUY UP TO 3 " T_COIN ": ");
        textn(edit, edit_len);
        break;
    }
    if (pr_kind == 'B')
        ask(step ? T_NULLROUTE " AND END THE GAME? Y/N"
                 : "LETTERS, BACKSPACE, ENTER TO BUY");
    mode = NORMAL;
}

/* One second short, as on the Apple II: the clock ticks 18 a second, not 18.2, so
 * its seconds run a few percent long; shown short, it never promises more time
 * than the server gives. */
void display_clock(unsigned char secs)
{
    if (!pr_kind)
        return;
    mode = bottom;
    at(ROW_ASK, 37);
    if (secs)
        number(secs - 1, 3, 0);
    else
        spaces(3);
    mode = NORMAL;
}

void display_message(const char *s)
{
    setup();
    mode = INV;
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(s);
    mode = NORMAL;
}

void display_game_over(void)
{
    mode = bottom;
    clear_row(ROW_INFO);
    at(ROW_INFO, 1);
    if (winners & (winners - 1))        /* a tie: the names need the room */
        text("TOP " T_PLAYER_P ":");
    else
        text("GAME OVER. TOP " T_PLAYER ":");
    for (i = 0; i < SEATS; ++i)
        if (winners >> i & 1) {
            put(' ');
            textn((const unsigned char *)seat_name[i], 8);
        }
    mode = NORMAL;
}

void display_link(void)
{
    if (!field)                         /* still on the title screen */
        return;
    mode = INV;
    at(0, COL_BOARD);                   /* over the grid */
    textn(region, region_len);
    put(':');
    text(link_up ? "CONNECTED" : "NO SIGNAL");
    mode = NORMAL;
}

void display_playfield(void)
{
    setup();
    field = 1;
    clear_screen();
    mode = INV;
    clear_row(0);
    at(0, 39 - (sizeof TH_TITLE - 1));
    text(TH_TITLE);
    mode = NORMAL;
    display_link();
    at(1, COL_BOARD);
    for (j = 0; j < RULE_COLS; ++j)
        put('A' + j);
    for (j = 0; j < RULE_ROWS; ++j) {
        at(ROW_BOARD + j, 0);
        put('0' + j);
    }
    for (j = 0; j < NODES; ++j)
        display_node(j);
    display_hives();
    at(ROW_SEATS - 1, 1);
    text(T_PLAYER);
    at(ROW_SEATS - 1, 18 - sizeof T_ACCOUNT + 1);
    text(T_ACCOUNT);
    for (j = 0; j < NSYN; ++j) {
        at(ROW_SEATS - 1, 20 + 3 * j);
        mode = HIVE;
        put(syn_letter[j]);
        mode = NORMAL;
    }
    for (i = 0; i < SEATS; ++i)
        display_seat(i);
    bottom = NORMAL;                    /* rows 22-23 are blank: no need to recolor */
    display_prompt();
}
#endif

/* ---------------------------------------------------------------- instructions
 * The pages are one file, SYNACK.HLP, in the current directory, made by
 * tools/help_msdos.py: page n is the 1920 bytes from (n-1) * 1920, its 24 rows
 * of 40 cells, character and attribute, as in video memory. While they're up the
 * game keeps drawing into `shadow`, a copy of the screen in memory (a monochrome
 * adapter has only the one text page), and closing copies it back. This code
 * knows nothing about how many pages there are: '>' past the last finds no page
 * there and stays put. They open again where they were left. */
#ifndef PC_SCREEN
#include <fcntl.h>
#include <io.h>
#endif

#define HELP_FILE   "SYNACK.HLP"
#define PAGE_CELLS  (24 * 40)
#define PAGE_BYTES  (PAGE_CELLS * 2)

static unsigned short shadow[25 * 80];
static cell *video;
static unsigned char page = 1;

/* Page `p` onto the screen; 0 if there is no such page. */
static unsigned char help_page(unsigned char p)
{
    static unsigned short buf[PAGE_CELLS];
    static const char none[] = "NO INSTRUCTIONS: " HELP_FILE, back[] = "ESC RETURNS TO THE GAME";
    unsigned short k;
    int got = 0;
#ifndef PC_SCREEN                       /* the PC capture build has no pages */
    int fd = open(HELP_FILE, O_RDONLY | O_BINARY);
    if (fd >= 0) {
        if (lseek(fd, (long)(p - 1) * PAGE_BYTES, SEEK_SET) >= 0)
            got = read(fd, buf, PAGE_BYTES);
        close(fd);
    }
#endif
    if (got != PAGE_BYTES) {
        if (p > 1)
            return 0;                   /* past the last page */
        for (k = 0; k < PAGE_CELLS; ++k)                /* no pages at all: say so */
            buf[k] = NORMAL << 8 | ' ';
        for (k = 0; none[k]; ++k)
            buf[11 * 40 + 6 + k] = NORMAL << 8 | none[k];
        for (k = 0; back[k]; ++k)
            buf[13 * 40 + 6 + k] = NORMAL << 8 | back[k];
    }
    for (k = 0; k < PAGE_CELLS; ++k) {
        v = buf[k] >> 8;
        if (colour)
            v = v == NORMAL ? C_TEXT : v == INV ? C_INV : v;
        video[k / 40 * cols + left + k % 40] = v << 8 | (buf[k] & 0xFF);
    }
    return 1;
}

void help_open(void)
{
    setup();
    video = screen;
    FMEMCPY(shadow, video, 25 * 2 * cols);      /* the game goes on drawing here */
    screen = shadow;
    helping = 1;
    help_page(page);
}

unsigned char help_key(unsigned char k)
{
    switch (k) {
    case '>':
        if (help_page(page + 1))
            ++page;
        return 1;
    case '<':
        if (page > 1 && help_page(page - 1))
            --page;
        return 1;
    case K_ESC:
    case K_RETURN:
    case K_HELP:
        FMEMCPY(video, shadow, 25 * 2 * cols);
        screen = video;
        helping = 0;
        return 0;
    }
    return 1;
}
