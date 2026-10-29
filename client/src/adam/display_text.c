/* The text-only display for the Adam: the NES's (src/nes/display_text.c) on
 * the TMS9918A's 32x24 cells of 8x8, all of them on the screen, written into
 * txt_cells and txt_pal and painted by text.c. Colour as on the NES:
 *
 *  inverse   the title bar, hives (letters), Black ICE sizes, the syndicate
 *            letters, whose turn it is, exploits that can't run, the title's
 *            key names
 *  palettes  the Grid and the seats on a green panel (the Atari's), cash and
 *            every other syndicate's crypto in yellow so neighbouring numbers
 *            stand apart; the bottom two rows magenta while this seat has a
 *            decision to make, and the title bar while the link is down
 *  flashing  the last move, for FLASH_SECS: inverse and back, by display_idle
 *  sprites   the controller's cursor (input.c's pick): under the exploit or
 *            the letter it would send, a box on the Grid node that exploit
 *            spikes, an arrow beside the syndicate a buy would take
 *
 * The playfield, by row:
 *   0      the title bar: region and link
 *   1-10   column letters, then the Grid's 9 rows; beside it the hives panel
 *          (letter, size, short name; while buying, price and crypto left), and
 *          LAST: under it
 *   11-17  the seats' header and the 6 seats: handle, cash, 7 holdings
 *   18-21  the log: each line on 1 row, or wrapped at a word onto 2
 *   22-23  what the open prompt is about; the question and the seconds left
 * Each call writes cells and marks their rows; the outermost call paints them
 * as it returns, so the playfield, built from the others, is painted once.
 *
 * Text arrives as ASCII $20-$5F; "\" from the server is money, and the font's
 * $5C is the yen sign. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"
#include "text.h"

#define NORMAL  0
#define INV     TXT_INV

#define FLASH_SECS  5
#define FLASH_TICKS 16                  /* inverse, then normal, this many ticks each (60 a second) */
#define FLASH_FLIPS (FLASH_SECS * 60 / FLASH_TICKS)

#define ROW_BAR     0
#define ROW_LETTERS 1
#define ROW_BOARD   2                   /* the Grid's rows 0-8: 2-10 */
#define COL_DIGIT   0
#define COL_BOARD   2                   /* its columns A-L: 2-13 */
#define COL_PANEL   15                  /* letter, size (3), gap, then from COL_NAME the short name */
#define COL_NAME    20                  /* or, while buying, price (5), crypto left (5) */
#define ROW_LAST    10
#define ROW_HEAD    11
#define ROW_SEATS   12
#define COL_CASH    10                  /* 6 columns */
#define COL_COINS   17                  /* 7 x 2 columns */
#define ROW_LOG     18
#define LOG_ROWS    4
#define LOG_INDENT  4                   /* a wrapped line goes on under its text, past "[$] " */
#define ROW_INFO    22
#define ROW_ASK     23
#define COL_CLOCK   29                  /* 3 columns, so a question is at most 28 */
#define WIDTH       32

/* The title's lines sit low, as on the Atari, whatever the logo's height */
#define ROW_REGION  15                  /* then the status, then HANDLE */
#define ROW_HANDLE  17
#define COL_VALUE   8
#define ROW_KEYS    18                  /* the keys and the controller's, 5 rows */
#define COL_PAD     8                   /* the controller's */
#define COL_KEYS    15                  /* what each does */
#define COL_HINTS   (COL_KEYS + 11)     /* ON/OFF after "MOVE HINTS" */

#include "title_art.h"                  /* title_art[], TITLE_END: tools/title_art.py n */
#if TITLE_END >= ROW_REGION - 1
#error "the title logo runs into the title's lines: at most rows 0-13"
#endif

extern unsigned char pick, dial;        /* input.c: the cursor, the handle's letter it is on */
void pick_check(void);                  /* input.c: the cursor to a new prompt's first answer */

static const char *const syn_short[NSYN] = SYN_SHORTS;

static unsigned char *cur;
static unsigned char mode = NORMAL, ready, i, j, n, v, nest;
unsigned char field;                    /* the playfield is up (input.c reads it) */
static unsigned char shown;             /* the display is on (txt_show) */
static unsigned char deciding;          /* rows 22-23 are magenta */

/* ---------------------------------------------------------------- drawing */

/* Each public call opens with begin() and closes with end(): the outermost
 * paints the rows written meanwhile. */
static void begin(void)
{
    if (!ready) {
        ready = 1;
        txt_init();
    }
    ++nest;
}

static void end(void)
{
    if (!--nest)
        txt_paint();
}

static void at(unsigned char r, unsigned char c)
{
    cur = txt_cells + ((unsigned)r << 5) + c;
    txt_dirty[r] = 1;
}

static void put(unsigned char ch)
{
    *cur++ = ch | mode;
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
    memset(txt_cells + ((unsigned)r << 5), ' ' | mode, TXT_COLS);
    txt_dirty[r] = 1;
}

static void clear_screen(void)
{
    memset(txt_cells, ' ', sizeof txt_cells);
    memset(txt_pal, PAL_TEXT, sizeof txt_pal);
    memset(txt_dirty, 1, sizeof txt_dirty);
}

/* Palette p for rows r0-r1, columns c0-c1. */
static void band(unsigned char r0, unsigned char r1, unsigned char c0, unsigned char c1,
                 unsigned char p)
{
    for (; r0 <= r1; ++r0) {
        memset(txt_pal + ((unsigned)r0 << 5) + c0, p, c1 - c0 + 1);
        txt_dirty[r0] = 1;
    }
}

/* Right-aligned decimal in `width` columns. `money` (units of 100) appends "00",
 * with the currency sign right before the digits when it is 1. */
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
        if (money == 1)
            ++len;                      /* the sign */
    }
    if (width > len)
        spaces(width - len);
    if (money == 1) {
        put('\\');
        --len;
    }
    textn(digits, len);
}

/* A seat's cash in 6 columns: "\99900", then the digits alone, then thousands and a K. */
static void cash6(unsigned value)
{
    if (value < 1000)
        number(value, 6, 1);
    else if (value < 10000)
        number(value, 6, 2);
    else {
        number(value / 10, 5, 0);
        put('K');
    }
}

static void node_name(unsigned char node)
{
    for (n = 0; node >= RULE_COLS; ++n)
        node -= RULE_COLS;
    put('A' + node);
    put('0' + n);
}

/* The syndicate letters in `mask`, each flipped from the row's mode, a space after each. */
static void syn_letters(unsigned char mask)
{
    for (i = 0; i < NSYN; ++i)
        if (mask >> i & 1) {
            mode ^= INV;
            put(syn_letter[i]);
            mode ^= INV;
            put(' ');
        }
}

/* ---------------------------------------------------------------- the controller's cursor */

static void sprite(unsigned char k, unsigned char row, unsigned char col, signed char dx,
                   signed char dy, unsigned char t)
{
    unsigned char *o = sat + (k << 2);
    o[0] = (row << 3) - 1 + dy;         /* a sprite shows a line below its y */
    o[1] = (col << 3) + dx;
    o[2] = t;
    o[3] = 0x0B;                        /* light yellow */
}

/* Where input.c's pick is: sprites 0-5, or all of them off (below the screen). */
void display_pick(void)
{
    begin();
    for (j = 0; j < SPRITES; ++j)
        sat[j << 2] = 0xC0;
    sat_dirty = 1;
    if (!field || leaving) {
        end();
        return;
    }
    switch (pr_kind) {
    case 'P':                           /* under "n:XX", and a box on the node */
        for (j = 0; j < 4; ++j)
            sprite(j, ROW_INFO, 5 * pick + j, 0, 0, SPR_LINE);
        v = kit[pick];
        if (v != NONE) {
            v &= 0x7F;
            for (n = 0; v >= RULE_COLS; ++n)
                v -= RULE_COLS;
            sprite(4, ROW_BOARD + n, COL_BOARD + v, -1, -1, SPR_TL);
            sprite(5, ROW_BOARD + n, COL_BOARD + v, 0, 0, SPR_BR);
        }
        break;
    case 'F':                           /* under the letter on the question row */
    case 'K':
        v = pr[pr_kind == 'F' ? 1 : 2];
        for (j = n = 0; j < pick; ++j)
            n += v >> j & 1;
        sprite(0, ROW_ASK, 2 * n, 0, 0, SPR_LINE);
        break;
    case 'B':                           /* beside the syndicate in the panel */
        if (!step)
            sprite(0, ROW_BOARD + pick, COL_PANEL - 1, 0, 0, SPR_ARROW);
        break;
    }
    end();
}

#if !FANCY_TITLE
/* ---------------------------------------------------------------- title */

void display_title_screen(void)
{
    const unsigned char *p;
    begin();
    field = 0;
    clear_screen();
    for (p = title_art; *p != 0xFF; p += 3 + p[2])
        memcpy(txt_cells + ((unsigned)p[0] << 5) + p[1], p + 3, p[2]);
    at(ROW_REGION, 1);
    text("REGION");                     /* from the server, once it answers */
    at(ROW_HANDLE, 1);
    text("HANDLE");
    mode = INV;
    at(ROW_KEYS, 1);
    text("TYPE");
    at(ROW_KEYS + 1, 1);
    text("RETURN");
    at(ROW_KEYS + 2, 1);
    put('I');
    at(ROW_KEYS + 3, 1);
    text("II");
    at(ROW_KEYS + 4, 1);
    text("III");
    at(ROW_KEYS, COL_PAD);
    text("STICK");
    at(ROW_KEYS + 1, COL_PAD);
    put('#');
    at(ROW_KEYS + 2, COL_PAD);
    put('*');
    at(ROW_KEYS + 3, COL_PAD);
    text("*+L");
    at(ROW_KEYS + 4, COL_PAD);
    text("*+R");
    mode = NORMAL;
    at(ROW_KEYS, COL_KEYS);
    text("YOUR HANDLE");
    at(ROW_KEYS + 1, COL_KEYS);
    text(T_JACKIN);
    at(ROW_KEYS + 2, COL_KEYS);
    text("INSTRUCTIONS");
    at(ROW_KEYS + 3, COL_KEYS);
    text("MOVE HINTS");
    at(ROW_KEYS + 4, COL_KEYS);
    text("TOGGLE SOUND");
    if (refused)                        /* a refusal stands out at the foot */
        band(ROW_ASK, ROW_ASK, 0, WIDTH - 1, PAL_ASK);
    at(ROW_ASK, 1);
    text(refused == REFUSED_LATE ? "TOO LATE: LAST SEAT TAKEN"
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN RETURN");
    display_title_update();
    display_pick();
    end();
    if (!help_up)
        txt_title(1);
    if (!shown) {                       /* start-up: the title appears whole */
        shown = 1;
        txt_show();
    }
}

/* Each field is padded to its widest, so drawing over the last one leaves nothing behind. */
void display_title_update(void)
{
    begin();
    at(ROW_REGION, COL_VALUE);
    textn(region, region_len);
    spaces(sizeof region - region_len);
    at(ROW_REGION + 1, 1);
    n = 0;
    if (status_len) {
        n = status_len - 1;
        textn(status_text, n);
    } else if (status_code == NO_CARRIER) {
        n = 10;
        text("NO CARRIER");
    }
    spaces(sizeof status - 2 - n);      /* 28 */
    at(ROW_HANDLE, COL_VALUE);          /* the dial's letter inverse, or past the end a space */
    v = dial > edit_len ? edit_len : dial;
    for (j = 0; j < 8; ++j) {
        mode = j == v ? INV : NORMAL;
        put(j < edit_len ? edit[j] : ' ');
    }
    mode = INV;
    at(ROW_KEYS + 3, COL_HINTS);
    text(hints ? "ON" : "OFF");
    mode = NORMAL;
    put(' ');
    end();
}
#endif

#if !FANCY_PLAYFIELD
/* ---------------------------------------------------------------- playfield */

/* The last move flashes from when it is first drawn until FLASH_FLIPS flips
 * have passed (display_idle); then `done` holds it, and it shows as usual. */
static unsigned char blink = NONE, blink_t, blink_n, blink_on, done = NONE;

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
    begin();
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
        m = INV;
    }
    if (node == last_node) {
        at(ROW_LAST, COL_PANEL);
        text("LAST: ");
        node_name(node);
        if (node != done) {
            if (blink != node) {        /* a new last move: inverse first */
                blink = node;
                blink_t = ticks();
                blink_n = FLASH_FLIPS;
                blink_on = 1;
            }
            if (blink_on)
                m ^= INV;
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    at(ROW_BOARD + r, COL_BOARD + c);
    *cur = v | m;
    end();
}

void display_idle(void)
{
    if (blink == NONE)
        return;
    if (blink != last_node) {
        blink = NONE;                   /* a newer move took over, or a new game */
        return;
    }
    if ((unsigned char)(ticks() - blink_t) < FLASH_TICKS)
        return;
    blink_t += FLASH_TICKS;
    blink_on ^= 1;
    if (!--blink_n) {
        done = blink;
        blink = NONE;
    }
    display_node(last_node);
}

void display_hives(void)
{
    begin();
    at(ROW_LETTERS, COL_PANEL);
    text(T_HIVE);                       /* over the letters and sizes */
    at(ROW_LETTERS, COL_NAME);
    text(pr_kind == 'B' ? "PRICE LEFT" : "SYNDICATES");
    for (j = 0; j < NSYN; ++j) {
        at(ROW_BOARD + j, COL_PANEL);
        mode = INV;
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
        put(' ');
        if (pr_kind == 'B') {           /* pr: timeout, cash[2], flags, price[7], supply[7] */
            n = pr[4 + j];
            if (n) {
                number(n, 5, 1);
                number(pr[11 + j], 5, 0);
            } else {
                spaces(10);
            }
            continue;
        }
        n = (unsigned char)strlen(syn_short[j]);
        text(syn_short[j]);
        spaces(10 - n);
    }
    end();
}

void display_seat(unsigned char seat)
{
    if (seat >= SEATS)
        return;
    begin();
    at(ROW_SEATS + seat, 0);
    n = seat_kind[seat];
    if (n == SEAT_EMPTY) {
        put(seat == me ? '>' : ' ');
        put('-');
        spaces(WIDTH - 2);
        end();
        return;
    }
    put(seat == me ? '>' : n == SEAT_AWAY ? '?' : n == SEAT_BOT ? '*' : ' ');
    if (seat == turn || (turn == NONE && winners >> seat & 1))
        mode = INV;                     /* whose turn it is; at the end, who came top */
    textn((const unsigned char *)seat_name[seat], 8);
    mode = NORMAL;
    put(' ');
    cash6(cash[seat]);
    put(' ');
    for (j = 0; j < NSYN; ++j) {
        n = coins[seat][j];
        if (n)
            number(n, 2, 0);
        else
            text(" .");
    }
    end();
}

/* A log line on 1 row, or on 2: as much as fits up to a space, then the rest
 * indented. The rows above move up as many. */
void display_log(void)
{
    unsigned char len = msg[1], cut = len, rows = 1;
    const unsigned char *s = msg + 2;
    if (len > WIDTH) {
        rows = 2;
        for (cut = WIDTH; cut && s[cut] != ' '; --cut)
            ;
        if (!cut)
            cut = WIDTH;
    }
    begin();
    memmove(txt_cells + ((unsigned)ROW_LOG << 5), txt_cells + ((unsigned)(ROW_LOG + rows) << 5),
            (LOG_ROWS - rows) << 5);
    for (i = 0; i < LOG_ROWS; ++i)
        txt_dirty[ROW_LOG + i] = 1;
    clear_row(ROW_LOG + LOG_ROWS - 1);
    if (rows == 2) {
        clear_row(ROW_LOG + LOG_ROWS - 2);
        at(ROW_LOG + LOG_ROWS - 2, 0);
        textn(s, cut);
        if (s[cut] == ' ')
            ++cut;
        s += cut;
        len -= cut;
        if (len > WIDTH - LOG_INDENT)
            len = WIDTH - LOG_INDENT;
        at(ROW_LOG + LOG_ROWS - 1, LOG_INDENT);
    } else {
        at(ROW_LOG + LOG_ROWS - 1, 0);
    }
    textn(s, len);
    end();
}

static void kit_line(void)
{
    clear_row(ROW_INFO);
    at(ROW_INFO, 0);
    for (j = 0; j < RULE_KITSIZE; ++j) {
        v = kit[j];
        put('1' + j);
        put(':');
        if (v == NONE) {
            text("--");
        } else {
            if (v & 0x80)               /* can't run now */
                mode = INV;
            node_name(v & 0x7F);
            mode = NORMAL;
        }
        put(' ');
    }
}

static void ask(const char *s)
{
    clear_row(ROW_ASK);
    at(ROW_ASK, 0);
    text(complaint ? complaint : s);
}

void display_prompt(void)
{
    begin();
    pick_check();
    v = pr_kind || turn == me || leaving;
    if (v != deciding) {
        deciding = v;
        band(ROW_INFO, ROW_ASK, 0, WIDTH - 1, v ? PAL_ASK : PAL_TEXT);
    }
    if (leaving) {                      /* the question waits; the prompt comes back after */
        ask(T_LOGOFF " NOW? Y/N");
        display_pick();
        end();
        return;
    }
    if (pr_kind == 'P' || (pr_kind == 0 && turn != NONE))
        kit_line();
    else if (pr_kind != 'S' && pr_kind != 0)
        clear_row(ROW_INFO);            /* in the lobby row 22 keeps the last result */
    at(ROW_INFO, 0);
    switch (pr_kind) {
    case 0:
        clear_row(ROW_ASK);
        if (turn < SEATS && turn != me) {
            at(ROW_ASK, 0);
            text("WAITING FOR ");
            textn((const unsigned char *)seat_name[turn], 8);
            at(ROW_ASK, 22);            /* the instructions, while there's time */
            text("I:HELP");
        }
        break;
    case 'S':
        ask("HOW MANY SEATS? 2-6  <");
        if (!complaint) {
            put('0' + pick);
            put('>');
        }
        break;
    case 'P':
        ask("RUN WHICH " T_EXPLOIT "? PRESS 1-6");
        break;
    case 'F':                           /* pr[1] = available */
        text(T_INFILTRATION "! " T_BROKER " TO WHICH?");
        ask("");
        syn_letters(pr[1]);
        text(" PRESS ONE");
        break;
    case 'K':                           /* pr[1] = ctx, pr[2] = options */
        text(pr[1] ? "TIE! ABSORB WHICH NEXT?" : "TIE! WHICH SURVIVES?");
        ask("");
        syn_letters(pr[2]);
        text(" PRESS ONE");
        break;
    case 'D':                           /* pr: absorbed, absorber, held, supply */
        text(T_HIVEJACK ": ");
        put(syn_letter[pr[2]]);
        text(" TAKES ");
        put(syn_letter[pr[1]]);
        text(". HOLD ");
        number(pr[3], 1, 0);
        ask(step ? T_SWAP " 2 FOR 1? <" : T_CASHOUT " HOW MANY? <");
        if (!complaint) {
            if (edit_len)
                textn(edit, edit_len);
            else
                put('0');               /* nothing typed is 0 */
            put('>');
        }
        break;
    case 'B':                           /* pr: cash[2], flags, price[7], supply[7] */
        if (!step || edit_len) {        /* at step 1 with nothing bought: just the question */
            text("BUY UP TO 3 " T_COIN ": ");
            textn(edit, edit_len);
        }
        ask(step ? T_NULLROUTE " NOW? Y/N" : "LETTERS, BS, RETURN TO BUY");
        break;
    }
    display_pick();
    end();
}

/* One second short, as on the Apple: shown short, it never promises more time
 * than the server gives. */
void display_clock(unsigned char secs)
{
    if (!pr_kind)
        return;
    begin();
    at(ROW_ASK, COL_CLOCK);
    if (secs)
        number(secs - 1, 3, 0);
    else
        spaces(3);
    end();
}

void display_message(const char *s)
{
    begin();
    mode = INV;
    clear_row(ROW_ASK);
    at(ROW_ASK, 0);
    text(s);
    mode = NORMAL;
    end();
}

/* The top netrunner on the info row; when several tie, as many handles as fit,
 * and all of them inverse in the seats. */
void display_game_over(void)
{
    begin();
    clear_row(ROW_INFO);
    at(ROW_INFO, 0);
    text(winners & (winners - 1) ? "TOP:" : "GAME OVER. TOP:");
    v = winners & (winners - 1) ? 4 : 15;   /* the column after it */
    for (i = 0; i < SEATS; ++i)
        if (winners >> i & 1) {
            for (n = 8; n && seat_name[i][n - 1] == ' '; --n)
                ;
            if (v + 1 + n > WIDTH)
                break;
            put(' ');
            textn((const unsigned char *)seat_name[i], n);
            v += 1 + n;
        }
    for (i = 0; i < SEATS; ++i)
        display_seat(i);
    end();
}

void display_link(void)
{
    if (!field)                         /* still on the title screen */
        return;
    begin();
    mode = link_up ? INV : NORMAL;      /* green, or white on magenta */
    band(ROW_BAR, ROW_BAR, 0, WIDTH - 1, link_up ? PAL_TEXT : PAL_ASK);
    clear_row(ROW_BAR);
    at(ROW_BAR, 1);
    textn(region, region_len);
    put(':');
    text(link_up ? "CONNECTED" : "NO SIGNAL");
    mode = NORMAL;
    end();
}

void display_playfield(void)
{
    txt_title(0);
    begin();
    field = 1;
    clear_screen();
    band(ROW_LETTERS, ROW_LAST, COL_BOARD - 1, COL_BOARD + RULE_COLS, PAL_PANEL);   /* the Grid's panel */
    band(ROW_SEATS, ROW_SEATS + 5, 0, WIDTH - 1, PAL_PANEL);                        /* the seats' band */
    band(ROW_SEATS, ROW_SEATS + 5, COL_CASH, COL_CASH + 5, PAL_PANEL2);
    for (j = 1; j < NSYN; j += 2)
        band(ROW_SEATS, ROW_SEATS + 5, COL_COINS + 2 * j, COL_COINS + 2 * j + 1, PAL_PANEL2);
    deciding = 0;
    display_link();
    at(ROW_LETTERS, COL_BOARD);
    for (j = 0; j < RULE_COLS; ++j)
        put('A' + j);
    for (j = 0; j < RULE_ROWS; ++j) {
        at(ROW_BOARD + j, COL_DIGIT);
        put('0' + j);
    }
    for (j = 0; j < NODES; ++j)
        display_node(j);
    display_hives();
    at(ROW_HEAD, 1);
    text(T_PLAYER);
    at(ROW_HEAD, COL_CASH + 6 - (sizeof T_ACCOUNT - 1));
    text(T_ACCOUNT);
    for (j = 0; j < NSYN; ++j) {
        at(ROW_HEAD, COL_COINS + 1 + 2 * j);
        mode = INV;
        put(syn_letter[j]);
        mode = NORMAL;
    }
    for (i = 0; i < SEATS; ++i)
        display_seat(i);
    display_prompt();
    end();
}
#endif
