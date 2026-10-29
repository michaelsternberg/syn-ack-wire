/* The text-only display for the NES: the game's screens in 30 columns of 8x8
 * tiles (columns 1-30, rows 1-28 of the name table, inside a TV's overscan),
 * written into txt_cells and copied to the PPU by the NMI (text.h, ppu.s). The
 * CoCo's display (src/coco/display_text.c) laid out anew, with colour where
 * the CoCo has only inverse:
 *
 *  inverse   the title bar, hives (letters), Black ICE sizes, the syndicate
 *            letters, whose turn it is, exploits that can't run, the title's
 *            key names
 *  palettes  the Grid and the seats on a green panel (the Atari's), cash and
 *            every other syndicate's crypto in yellow so neighbouring numbers
 *            stand apart; the bottom two rows purple while this seat has a
 *            decision to make, and the title bar while the link is down
 *  flashing  the last move, for FLASH_SECS: inverse and back, by display_idle
 *  sprites   the pad's cursor (input.c's pick): under the exploit or the
 *            letter it would send, a box on the Grid node that exploit spikes,
 *            an arrow beside the syndicate a buy would take
 *
 * The playfield, rows by name table row:
 *   1      the title bar: region and link
 *   2-11   column letters, then the Grid's 9 rows; beside it the hives panel
 *          (letter, size, short name; while buying, price and crypto left), and
 *          LAST: under it
 *   13-19  the seats' header and the 6 seats: handle, cash, 7 holdings
 *   20-25  the log: each line on 1 row, or wrapped at a word onto 2
 *   26-27  what the open prompt is about; the question and the seconds left
 * Each call writes cells and notes their rows; the outermost call marks them
 * for the NMI as it returns, so the playfield, built from the others, goes
 * out once and a half-written row never does.
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

#define ROW_BAR     1
#define ROW_LETTERS 2
#define ROW_BOARD   3                   /* the Grid's rows 0-8: 3-11 */
#define COL_DIGIT   1
#define COL_BOARD   3                   /* its columns A-L: 3-14 */
#define COL_PANEL   16                  /* letter, size (3), gap, then from COL_NAME the short name */
#define COL_NAME    21                  /* or, while buying, price (5), gap, crypto left (2) */
#define ROW_LAST    11
#define ROW_HEAD    13
#define ROW_SEATS   14
#define COL_CASH    10                  /* 6 columns */
#define COL_COINS   16                  /* 7 x 2 columns, each its own palette block */
#define ROW_LOG     20
#define LOG_ROWS    6
#define LOG_INDENT  4                   /* a wrapped line goes on under its text, past "[$] " */
#define ROW_INFO    26
#define ROW_ASK     27
#define COL_CLOCK   28                  /* 3 columns, so a question is at most 26 */
#define WIDTH       30                  /* columns 1-30 */

/* The title's lines sit low, as on the Atari, whatever the logo's height */
#define ROW_REGION  16                  /* then the status, then HANDLE */
#define ROW_HANDLE  18
#define COL_VALUE   9
#define ROW_KEYS    20                  /* the pad's buttons, 6 rows */
#define COL_KEYS    10                  /* what each does */
#define COL_HINTS   (COL_KEYS + 13)     /* ON/OFF after "MOVE HINTS" */

/* palette blocks are 2x2 tiles: block rows and columns */
#define BR(r)       ((r) >> 1)
#define BC(c)       ((c) >> 1)

#include "title_art.h"                  /* title_art[], TITLE_END: tools/title_art.py n */
#if TITLE_END >= ROW_REGION - 1
#error "the title logo runs into the title's lines: at most rows 1-14"
#endif

extern unsigned char pick, dial;        /* input.c: the pad's cursor, the handle's letter it is on */
extern unsigned char help_up;           /* help.c: the instructions are up */

static const char *const syn_short[NSYN] = SYN_SHORTS;

static unsigned char *cur;
static unsigned char mode = NORMAL, ready, i, j, n, v, nest;
unsigned char field;                    /* the playfield is up (input.c reads it) */
static unsigned char shown;             /* the display is on (txt_show) */
static unsigned char deciding;          /* rows 26-27 are purple */
static unsigned char pend[TXT_ROWS];

/* ---------------------------------------------------------------- drawing */

/* Each public call opens with begin() and closes with end(): the outermost
 * marks the rows written meanwhile, for the NMI. */
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
    if (--nest)
        return;
    for (i = 0; i < TXT_ROWS; ++i)
        if (pend[i]) {
            pend[i] = 0;
            txt_dirty[i] = 1;
        }
    any_dirty = 1;
}

static void at(unsigned char r, unsigned char c)
{
    cur = txt_cells + ((unsigned)r << 5) + c;
    pend[r] = 1;
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
    pend[r] = 1;
}

static void clear_screen(void)
{
    memset(txt_cells, ' ', sizeof txt_cells);
    memset(pend, 1, sizeof pend);
    memset(attr, PAL_TEXT, 64);
    memset(attr_dirty, 1, 8);
}

/* Palette p for the blocks of rows br0-br1 and columns bc0-bc1. */
static void band(unsigned char br0, unsigned char br1, unsigned char bc0, unsigned char bc1,
                 unsigned char p)
{
    unsigned char *a, sh, c;
    for (; br0 <= br1; ++br0) {
        for (c = bc0; c <= bc1; ++c) {
            a = attr + (br0 >> 1 << 3) + (c >> 1);
            sh = (br0 & 1) << 2 | (c & 1) << 1;
            *a = (*a & ~(3 << sh)) | p << sh;
        }
        attr_dirty[br0 >> 1] = 1;
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

/* ---------------------------------------------------------------- the pad's cursor */

static void sprite(unsigned char k, unsigned char row, unsigned char col, signed char dx,
                   signed char dy, unsigned char t)
{
    unsigned char *o = OAM + (k << 2);
    o[0] = (row << 3) - 1 + dy;         /* a sprite shows a line below its y */
    o[1] = t;
    o[2] = 0;
    o[3] = (col << 3) + dx;
}

/* Where input.c's pick is: sprites 0-5, or all of them off. */
void display_pick(void)
{
    memset(OAM, 0xF8, 6 * 4);
    if (!field || leaving)
        return;
    switch (pr_kind) {
    case 'P':                           /* under "n:XX", and a box on the node */
        for (j = 0; j < 4; ++j)
            sprite(j, ROW_INFO, 1 + 5 * pick + j, 0, 0, SPR_LINE);
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
        sprite(0, ROW_ASK, 1 + 2 * n, 0, 0, SPR_LINE);
        break;
    case 'B':                           /* beside the syndicate in the panel */
        if (!step)
            sprite(0, ROW_BOARD + pick, COL_PANEL - 1, 0, 0, SPR_ARROW);
        break;
    }
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
    at(ROW_REGION, 2);
    text("REGION");                     /* from the server, once it answers */
    at(ROW_HANDLE, 2);
    text("HANDLE");
    mode = INV;
    at(ROW_KEYS, 2);
    text("D-PAD");
    at(ROW_KEYS + 1, 2);
    put('B');
    at(ROW_KEYS + 2, 2);
    text("START");
    at(ROW_KEYS + 3, 2);
    text("SELECT");
    at(ROW_KEYS + 4, 2);
    text("SEL+A");
    at(ROW_KEYS + 5, 2);
    text("SEL+B");
    mode = NORMAL;
    at(ROW_KEYS, COL_KEYS);
    text("PICK THE LETTERS");
    at(ROW_KEYS + 1, COL_KEYS);
    text("DELETE ONE");
    at(ROW_KEYS + 2, COL_KEYS);
    text(T_JACKIN);
    at(ROW_KEYS + 3, COL_KEYS);
    text("INSTRUCTIONS");
    at(ROW_KEYS + 4, COL_KEYS);
    text("MOVE HINTS");
    at(ROW_KEYS + 5, COL_KEYS);
    text("TOGGLE SOUND");
    if (refused)                        /* a refusal stands out at the foot */
        band(BR(ROW_ASK), BR(ROW_ASK), 0, 15, PAL_ASK);
    at(ROW_ASK, 1);
    text(refused == REFUSED_LATE ? "TOO LATE: LAST SEAT TAKEN"
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "PICK A HANDLE, THEN START");
    display_title_update();
    display_pick();
    noise_on = !help_up;
    end();
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
    at(ROW_REGION + 1, 2);
    n = 0;
    if (status_len) {
        n = status_len - 1;
        textn(status_text, n);
    } else if (status_code == NO_CARRIER) {
        n = 10;
        text("NO CARRIER");
    }
    spaces(sizeof status - 2 - n);      /* 28: to the end of the row */
    at(ROW_HANDLE, COL_VALUE);          /* the dial's letter inverse, or past the end a space */
    for (j = 0; j < 8; ++j) {
        mode = j == dial ? INV : NORMAL;
        put(j < edit_len ? edit[j] : ' ');
    }
    mode = INV;
    at(ROW_KEYS + 4, COL_HINTS);
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
    at(ROW_SEATS + seat, 1);
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
    cash6(cash[seat]);
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
        pend[ROW_LOG + i] = 1;
    clear_row(ROW_LOG + LOG_ROWS - 1);
    if (rows == 2) {
        clear_row(ROW_LOG + LOG_ROWS - 2);
        at(ROW_LOG + LOG_ROWS - 2, 1);
        textn(s, cut);
        if (s[cut] == ' ')
            ++cut;
        s += cut;
        len -= cut;
        if (len > WIDTH - LOG_INDENT)
            len = WIDTH - LOG_INDENT;
        at(ROW_LOG + LOG_ROWS - 1, 1 + LOG_INDENT);
    } else {
        at(ROW_LOG + LOG_ROWS - 1, 1);
    }
    textn(s, len);
    end();
}

static void kit_line(void)
{
    clear_row(ROW_INFO);
    at(ROW_INFO, 1);
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
    at(ROW_ASK, 1);
    text(complaint ? complaint : s);
}

void display_prompt(void)
{
    begin();
    v = pr_kind || turn == me || leaving;
    if (v != deciding) {
        deciding = v;
        band(BR(ROW_INFO), BR(ROW_INFO), 0, 15, v ? PAL_ASK : PAL_TEXT);
    }
    if (leaving) {                      /* the question waits; the prompt comes back after */
        ask(T_LOGOFF " NOW? A:YES B:NO");
        display_pick();
        end();
        return;
    }
    if (pr_kind == 'P' || (pr_kind == 0 && turn != NONE))
        kit_line();
    else if (pr_kind != 'S' && pr_kind != 0)
        clear_row(ROW_INFO);            /* in the lobby row 26 keeps the last result */
    at(ROW_INFO, 1);
    switch (pr_kind) {
    case 0:
        clear_row(ROW_ASK);
        if (turn < SEATS && turn != me) {
            at(ROW_ASK, 1);
            text("WAITING FOR ");
            textn((const unsigned char *)seat_name[turn], 8);
            at(ROW_ASK, 23);            /* the instructions, while there's time */
            text("SEL:HELP");
        }
        break;
    case 'S':
        ask("SEATS: <");
        if (!complaint) {
            put('0' + pick);
            text(">   A: START");
        }
        break;
    case 'P':
        ask("RUN WHICH " T_EXPLOIT "? A: RUN");
        break;
    case 'F':                           /* pr[1] = available */
        text(T_INFILTRATION "! " T_BROKER " TO WHICH?");
        ask("");
        syn_letters(pr[1]);
        text(" A: PICK");
        break;
    case 'K':                           /* pr[1] = ctx, pr[2] = options */
        text(pr[1] ? "TIE! ABSORB WHICH NEXT?" : "TIE! WHICH SURVIVES?");
        ask("");
        syn_letters(pr[2]);
        text(" A: PICK");
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
            textn(edit, edit_len);
            put('>');
        }
        break;
    case 'B':                           /* pr: cash[2], flags, price[7], supply[7] */
        if (!step || edit_len) {        /* at step 1 with nothing bought: just the question */
            text("BUY UP TO 3 " T_COIN ": ");
            textn(edit, edit_len);
        }
        ask(step ? T_NULLROUTE " NOW? A:YES B:NO" : "A ADD, B UNDO, START BUY");
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
    at(ROW_ASK, 1);
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
    at(ROW_INFO, 1);
    text(winners & (winners - 1) ? "TOP:" : "GAME OVER. TOP:");
    v = winners & (winners - 1) ? 5 : 16;   /* the column after it */
    for (i = 0; i < SEATS; ++i)
        if (winners >> i & 1) {
            for (n = 8; n && seat_name[i][n - 1] == ' '; --n)
                ;
            if (v + 1 + n > WIDTH + 1)
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
    mode = link_up ? INV : NORMAL;      /* green, or white on purple */
    band(BR(ROW_BAR), BR(ROW_BAR), 0, 15, link_up ? PAL_TEXT : PAL_ASK);
    clear_row(ROW_BAR);
    at(ROW_BAR, 2);
    textn(region, region_len);
    put(':');
    text(link_up ? "CONNECTED" : "NO SIGNAL");
    mode = NORMAL;
    end();
}

void display_playfield(void)
{
    begin();
    field = 1;
    noise_on = 0;
    clear_screen();
    band(BR(ROW_LETTERS), BR(ROW_LAST), BC(2), BC(15), PAL_PANEL);  /* the Grid's panel */
    band(BR(ROW_SEATS), BR(ROW_SEATS + 5), 0, 15, PAL_PANEL);       /* the seats' band */
    band(BR(ROW_SEATS), BR(ROW_SEATS + 5), BC(COL_CASH), BC(COL_COINS - 1), PAL_PANEL2);
    for (j = 1; j < NSYN; j += 2)
        band(BR(ROW_SEATS), BR(ROW_SEATS + 5), BC(COL_COINS) + j, BC(COL_COINS) + j, PAL_PANEL2);
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
    at(ROW_HEAD, COL_COINS - (sizeof T_ACCOUNT - 1));
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
