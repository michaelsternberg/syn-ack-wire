/* The text-only display for the Heathkit H89: the 40x24 layout in the middle of the H19 terminal's 80x24
 * (columns 20-59), the circuit board either side (margins.txt). The layout and most of the code are
 * MS-DOS's (src/msdos/display_text.c), in its monochrome attributes:
 *
 *  normal   the Grid's empty nodes (.), lone spikes (+) and move hints (1-6), the panel, the seats, the
 *           log; rows 22-23 while others play
 *  reverse  the title bar, hives (letters), Black ICE sizes, the syndicate letters, whose turn it is; rows
 *           22-23 while this seat has a decision to make (inside them, what normally shows reverse shows
 *           normal)
 *  the last move flips between the two, a few times a second, for FLASH_SECS (the H19 has no blink)
 *
 * The H19 is a terminal on a serial line: the screen is drawn into memory (vch[] and vat[], a character
 * and its attributes per cell), and display_flush(), called with every key check (input.c), sends the H19
 * only the cells that changed since, each run of them after a cursor move (ESC Y), switching reverse video
 * (ESC p, ESC q) and the graphics characters (ESC F, ESC G) as the cells need. The graphics characters
 * take the place of the codes 5Eh-7Eh, lower case among them, so a cell of text there (the help pages'
 * lower case) has graphics off; text elsewhere doesn't mind. Text arrives as ASCII; "\" from the server
 * (the Atari's yen sign) shows as Y, as on the Apple II. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"
#include "h89.h"

#define NORMAL  0x00
#define INV     0x01                    /* reverse video */
#define GFX     0x02                    /* the H19's graphics characters */
#define HIVE    INV                     /* a hive's letter */
#define FLIP    INV                     /* mode ^ FLIP: normal <-> reverse */
#define YEN     'Y'

#define FLASH_SECS  4                   /* in ticks it must fit a byte: 4 * 62 */

#define ROW_BOARD   2
#define COL_BOARD   2
#define COL_PANEL   15                  /* 25 columns: HIVES letter and size, gap, name (15) */
#define COL_NAME    24                  /* or, while buying, price and crypto left */
#define ROW_SEATS   13
#define ROW_LOG     19
#define LOG_ROWS    3
#define ROW_INFO    22
#define ROW_ASK     23

#define ROW_REGION  15                  /* then STATUS and HANDLE */
#define ROW_KEYS    18                  /* F2, F3, F1 */
#define COL_KEYS    18                  /* what each key does */
#define COL_HINTS   (COL_KEYS + 11)     /* ON/OFF after "MOVE HINTS" */

#define ROWS    24
#define COLS    80
#define LEFT    20                      /* where the game's 40 columns start */
#define CELLS   (ROWS * COLS)

#include "screens.h"                    /* margins[], MARGIN_W, title_art[], TITLE_END */

#ifdef PC_SCREEN                        /* the PC build for the manual's captures dumps them (test/pc_harness.c) */
unsigned char vch[CELLS], vat[CELLS];
#else
static unsigned char vch[CELLS], vat[CELLS];    /* the screen as the H19 should show it */
#endif
static unsigned char sch[CELLS], sat[CELLS];    /* as the H19 shows it now */
static unsigned char dirty[ROWS];               /* rows with a cell changed since the last flush */
static unsigned char *ch = vch, *at_ = vat;     /* what the game draws into: the screen, or under help */
static unsigned cur;                    /* the cell put() writes next */
static unsigned char cr;                /* its row */
static unsigned char helping;           /* the instructions are up */
static unsigned char mode = NORMAL, ready, i, j, n, v;
static unsigned char field;             /* the playfield is up: the title bar may draw */
static unsigned char bottom = NORMAL;   /* rows 22-23: INV while this seat decides */

/* ---------------------------------------------------------------- the terminal */

static unsigned char t_row = 0xFF, t_col, t_at = 0xFF;  /* the H19's cursor and modes, 0xFF unknown */

static void out(unsigned char c)
{
    bdos(6, c);
}

static void esc(unsigned char c)
{
    out(0x1B);
    out(c);
}

/* The H19's reverse and graphics as `a` asks; graphics only matters for the codes it replaces. */
static void modes(unsigned char a, unsigned char c)
{
    if (t_at == 0xFF) {                 /* unknown: set both */
        esc('q');
        esc('G');
        t_at = NORMAL;
    }
    if ((a ^ t_at) & INV) {
        esc(a & INV ? 'p' : 'q');
        t_at ^= INV;
    }
    if (c >= 0x5E && ((a ^ t_at) & GFX)) {
        esc(a & GFX ? 'F' : 'G');
        t_at ^= GFX;
    }
}

void display_flush(void)
{
    unsigned k;
    unsigned char r, c, a;
    for (r = 0; r < ROWS; ++r) {
        if (!dirty[r])
            continue;
        dirty[r] = 0;
        k = r * COLS;
        for (c = 0; c < COLS; ++c, ++k) {
            a = vat[k] & (INV | GFX);
            if (vch[k] == sch[k] && a == sat[k])
                continue;
            if (r != t_row || c != t_col) {
                esc('Y');
                out(32 + r);
                out(32 + c);
                t_row = r;
            }
            modes(a, vch[k]);
            out(vch[k]);
            sch[k] = vch[k];
            sat[k] = a;
            t_col = c + 1;              /* at column 80 the H19 stays put (ESC w): a move comes next */
        }
    }
}

static unsigned char read_page(unsigned char p);

/* The H19 cleared, wrapping off, the cursor hidden, the first help page read: once. */
static void setup(void)
{
    if (ready)
        return;
    ready = 1;
    esc('E');
    esc('w');
    out(0x1B);
    out('x');
    out('5');
    memset(vch, ' ', CELLS);
    memset(sch, ' ', CELLS);            /* as ESC E left it */
    memset(sat, NORMAL, CELLS);
    read_page(1);                       /* while the drive still turns from loading the program */
}

/* On the way out to CP/M: the H19 as CP/M likes it, cleared. */
void display_restore(void)
{
    if (!ready)
        return;
    esc('q');
    esc('G');
    esc('v');
    out(0x1B);
    out('y');
    out('5');
    esc('E');
}

/* ---------------------------------------------------------------- drawing */

static void at(unsigned char r, unsigned char c)
{
    cur = r * COLS + LEFT + c;
    cr = r;
}

static void cell(unsigned k, unsigned char c, unsigned char a)
{
    ch[k] = c;
    at_[k] = a;
    dirty[k / COLS] = 1;
}

static void put(unsigned char c)
{
    if (c == '\\')
        c = YEN;
    ch[cur] = c;
    at_[cur++] = mode;
    dirty[cr] = 1;
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

/* Graphics cells, an H19 code each with bit 7 for reverse (screens.h). */
static void art(unsigned k, const unsigned char *p, unsigned char len)
{
    for (; len--; ++k, ++p)
        cell(k, *p & 0x7F, GFX | (*p >> 7));
}

static void clear_screen(void)
{
    mode = NORMAL;
    memset(ch, ' ', CELLS);
    memset(at_, NORMAL, CELLS);
    memset(dirty, 1, ROWS);
}

static void draw_margins(void)
{
    unsigned char r;
    for (r = 0; r < ROWS; ++r) {
        art(r * COLS, margins[r], MARGIN_W);
        art(r * COLS + COLS - MARGIN_W, margins[r] + MARGIN_W, MARGIN_W);
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
            mode ^= FLIP;
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
    for (p = title_art; *p != 0xFF; p += 3 + p[2])
        art(p[0] * COLS + p[1], p + 3, p[2]);
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
    mode = NORMAL;
    at(ROW_KEYS, COL_KEYS);
    text("MOVE HINTS");
    at(ROW_KEYS + 1, COL_KEYS);
    text("TOGGLE SOUND");
    at(ROW_KEYS + 2, COL_KEYS);
    text("INSTRUCTIONS, ANY TIME");
    if (refused)
        mode = INV;                     /* a refusal stands out at the foot */
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN"
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN RETURN. ESC QUITS");
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

/* The last move flips from when it is first drawn until FLASH_SECS have
 * passed (display_idle); then `done` holds it, and it shows as usual. */
static unsigned char blink = NONE, blink_t, done = NONE, lit;

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
                lit = 1;
            }
            if (lit)
                m ^= FLIP;
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    cell((ROW_BOARD + r) * COLS + LEFT + COL_BOARD + c, v, m);
}

void display_idle(void)
{
    if (blink == NONE)
        return;
    if (blink != last_node)
        blink = NONE;                   /* a newer move took over, or a new game */
    else if ((unsigned char)(v = ticks() - blink_t) >= FLASH_SECS * TICKS_PER_SEC) {
        done = blink;
        blink = NONE;
        display_node(done);
    } else if ((v >> 4 & 1) == lit) {  /* lit and unlit by turns, about twice a second */
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
    unsigned k;
    for (i = 0; i < LOG_ROWS - 1; ++i) {
        k = (ROW_LOG + i) * COLS + LEFT;
        memcpy(ch + k, ch + k + COLS, 40);
        memcpy(at_ + k, at_ + k + COLS, 40);
        dirty[ROW_LOG + i] = 1;
    }
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
    unsigned k = ROW_INFO * COLS + LEFT;
    for (i = 0; i < 40; ++i, ++k)
        at_[k] ^= INV;                  /* every cell there was in the old mode, or flipped from it */
    dirty[ROW_INFO] = 1;
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
                 : "LETTERS, BACKSPACE, RETURN TO BUY");
    mode = NORMAL;
}

/* One second short: the clock's second is 62 ticks, not 62.5, so it runs a little long; shown short, it
 * never promises more time than the server gives. */
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
    draw_margins();
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
 * The pages are one file, SYNACK.HLP, on the disk the program came from (CP/M's current drive), made by
 * tools/h89/help.py: page n is the 1920 bytes from (n-1) * 1920, its 24 rows of 40 cells, each a character
 * and its attributes (INV, GFX). While they're up the game goes on drawing into the cells under them
 * (uch[], uat[]), and closing puts those back. '>' past the last page finds none and stays put. They open
 * again where they were left.
 *
 * The H17's motor is off by the time a player asks, and waiting for it was most of the wait for a page: page
 * 1 is read at start (setup), while the drive still turns, and each page, once on the screen, reads the next
 * one ahead into a second buffer. The file is opened once. */
#define PAGE_CELLS  (24 * 40)
#define PAGE_BYTES  (PAGE_CELLS * 2)
#define PAGE_RECS   (PAGE_BYTES / 128)  /* CP/M reads 128-byte records */

static unsigned char uch[CELLS], uat[CELLS];
static unsigned char page = 1;

/* CP/M's file control block for SYNACK.HLP, read with random reads (BDOS 33) from record r0-r2. */
static unsigned char fcb[36] = { 0, 'S', 'Y', 'N', 'A', 'C', 'K', ' ', ' ', 'H', 'L', 'P' };
static unsigned char buf[2][PAGE_BYTES];        /* two pages: the one up, and the next, read ahead */
static unsigned char held[2];           /* the page each holds, 0 none */
static unsigned char up = 0xFF;         /* the one on the screen */
static unsigned char opened;            /* 1 open, 2 not on the disk */

/* Page `p` into a buffer, not the one on the screen; its number, or 0xFF if there is no such page. */
static unsigned char read_page(unsigned char p)
{
    unsigned rec;
    unsigned char got, b;
    for (b = 0; b < 2; ++b)
        if (held[b] == p)
            return b;
    if (!opened)
        opened = bdos(15, (int)fcb) != 0xFF ? 1 : 2;
    if (opened != 1)
        return 0xFF;
    b = up == 0;                        /* the other one */
    held[b] = 0;                        /* overwritten from here */
    rec = (p - 1) * PAGE_RECS;
    for (got = 0; got < PAGE_RECS; ++got, ++rec) {
        bdos(26, (int)(buf[b] + got * 128));    /* the DMA address */
        fcb[33] = rec;
        fcb[34] = rec >> 8;
        fcb[35] = 0;
        if (bdos(33, (int)fcb) != 0)
            break;
    }
    bdos(26, 0x80);
    if (got != PAGE_RECS)
        return 0xFF;
    held[b] = p;
    return b;
}

/* Page `p` onto the screen, sent at once, then the next read ahead while it is read; 0 if there is no
 * such page. */
static unsigned char help_page(unsigned char p)
{
    static const char none[] = "NO INSTRUCTIONS: SYNACK.HLP", back[] = "ESC RETURNS TO THE GAME";
    unsigned k;
    unsigned char *q;
    unsigned char b = read_page(p);
    if (b == 0xFF) {
        if (p > 1)
            return 0;                   /* past the last page */
        b = up == 0;                    /* no pages at all: say so */
        held[b] = 0;
        q = buf[b];
        for (k = 0; k < PAGE_CELLS; ++k) {
            q[2 * k] = ' ';
            q[2 * k + 1] = NORMAL;
        }
        for (k = 0; none[k]; ++k)
            q[2 * (11 * 40 + 6 + k)] = none[k];
        for (k = 0; back[k]; ++k)
            q[2 * (13 * 40 + 6 + k)] = back[k];
    }
    up = b;
    q = buf[b];
    for (k = 0; k < PAGE_CELLS; ++k) {
        unsigned c = k / 40 * COLS + LEFT + k % 40;
        vch[c] = q[2 * k];
        vat[c] = q[2 * k + 1];
    }
    memset(dirty, 1, ROWS);
    display_flush();
    read_page(p + 1);
    return 1;
}

void help_open(void)
{
    setup();
    memcpy(uch, vch, CELLS);            /* the game goes on drawing here */
    memcpy(uat, vat, CELLS);
    ch = uch;
    at_ = uat;
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
        memcpy(vch, uch, CELLS);
        memcpy(vat, uat, CELLS);
        memset(dirty, 1, ROWS);
        ch = vch;
        at_ = vat;
        helping = 0;
        display_flush();                /* at once, as the pages are */
        return 0;
    }
    return 1;
}
