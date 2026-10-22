/* The text-only display for the Atari: the OS's 40x24 text screen, written
 * straight to screen memory. Hives are inverse letters; the last move, the
 * player whose turn it is and the prompt line are inverse too.
 *
 *  row  0    NEO-KANSAI-1:CONNECTED    SYN-ACK WIRE   (or NO SIGNAL)
 *  row  1    ABCDEFGHIJKL   HIVES  SYNDICATES
 *  rows 2-10 0 ..+GG.3....    G 12  GHOST//MODEM   (one per syndicate)
 *                         LAST: C4
 *  1-6 = an empty node that exploit in your kit can run on (when hints are on)
 *  row 11  blank
 *  row 12  NETRUNNER    ACCT  G  R  D  I  B  L  O
 *  rows 13-18 >NAME    ?  \6000  3  .  .  1  .  .  .    (one per seat)
 *  rows 19-21 log
 *  row 22  kit, or what the open prompt is about
 *  row 23  the question, and seconds left
 *
 * Behind the Grid sits a panel: two quad-width players in front of the
 * playfield. A GR.0 letter takes only its luminance from COLOR1 and its hue
 * from what's behind it, so the letters stay readable in the panel's hue: the
 * background green one step brighter, a subtle shift.
 *
 * On the title screen the same players, four of them now, fill the art's
 * letters with static (snow.s).
 *
 * Text arrives as ASCII $20-$5F; screen memory wants internal codes, which for
 * that range is the ASCII value minus $20. "\" ($5C) is redrawn as the yen sign.
 * Each screen can be replaced by a hand-made one: see config.h. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"

#define SAVMSC  (*(unsigned char **)0x58)
#define CRSINH  (*(unsigned char *)0x2F0)
#define CHBAS   (*(unsigned char *)0x2F4)
#define COLOR1  (*(unsigned char *)0x2C5)
#define COLOR2  (*(unsigned char *)0x2C6)
#define COLOR4  (*(unsigned char *)0x2C8)
#define PCOLR0  ((unsigned char *)0x2C0)   /* PCOLR0-3 */
#define SDMCTL  (*(unsigned char *)0x22F)
#define GPRIOR  (*(unsigned char *)0x26F)
#define HPOSP0  ((unsigned char *)0xD000)  /* HPOSP0-3 */
#define SIZEP0  ((unsigned char *)0xD008)  /* SIZEP0-3 */
#define GRACTL  (*(unsigned char *)0xD01D)
#define PMBASE  (*(unsigned char *)0xD407)
#define ROMFONT ((const unsigned char *)0xE000)

#define INV 0x80

#define ROW_BOARD   2
#define COL_BOARD   2
#define COL_PANEL   15                  /* 25 columns: HIVES letter and size, gap, name (15) */
#define COL_NAME    24                  /* or, while buying, price and crypto left */
#define ROW_SEATS   13
#define ROW_LOG     19
#define LOG_ROWS    3
#define ROW_INFO    22
#define ROW_ASK     23

/* The panel. Double-line player memory: a byte is 2 scan lines, and text row 0
 * starts at scan line 32. A quad-width player bit is one character (4 color
 * clocks); column 0 is at color clock 48. Player 0 covers 8 columns, player 1
 * the other 4 ($F0). */
#define PM_TOP      (16 + 4 * ROW_BOARD)
#define PM_LEFT     (48 + 4 * COL_BOARD)
#define PM_COLOR    0xC2                /* COLOR2, one luminance step up */
#define SNOW_COLOR  0x08                /* gray: a dead channel */

#include "title_art.h"                  /* title_art[], TITLE_END, SNOW_LEFT */

extern unsigned char snow_on;           /* snow.s */
void snow_init(void);

#pragma bss-name (push, "FONT")
static unsigned char font[1024];
#pragma bss-name (pop)
#pragma bss-name (push, "PM")
unsigned char pm[1024];                 /* players from pm + $200, 128 bytes each */
#pragma bss-name (pop)

static unsigned char *row[24];
static unsigned char *cur;
static unsigned char inv, ready, i, j, n, v;
static unsigned char field;             /* the playfield is up: the title bar may draw */

static const unsigned char yen[8] = { 0x00, 0x66, 0x66, 0x3C, 0x7E, 0x18, 0x7E, 0x18 };

/* ---------------------------------------------------------------- drawing */

static void setup(void)
{
    unsigned char *p;
    if (ready)
        return;
    ready = 1;
    p = SAVMSC;
    for (i = 0; i < 24; ++i) {
        row[i] = p;
        p += 40;
    }
    memcpy(font, ROMFONT, sizeof font);
    memcpy(font + 0x3C * 8, yen, 8);    /* "\" -> yen */
    CHBAS = (unsigned)font >> 8;
    memset(pm, 0, sizeof pm);           /* not zeroed at startup: only BSS is */
    PMBASE = (unsigned)pm >> 8;
    memset(SIZEP0, 3, 4);               /* quad width */
    snow_init();
    GPRIOR = 1;                         /* players in front of the playfield */
    SDMCTL |= 0x08;                     /* player DMA, double-line */
    GRACTL = 2;
    CRSINH = 1;
    COLOR2 = 0xC0;                      /* green on near-black */
    COLOR1 = 0x0C;
    COLOR4 = 0x00;
}

/* The players: static in the title's letters, or the panel behind the Grid. */
static const unsigned char hpos[2][4] = {
    { SNOW_LEFT, SNOW_LEFT + 32, SNOW_LEFT + 64, SNOW_LEFT + 96 },
    { PM_LEFT, PM_LEFT + 32, 0, 0 }     /* 0 = off screen */
};

static void panel(unsigned char on)
{
    snow_on = !on;                      /* first: the VBI keeps out of the players */
    if (on != field) {
        memset(pm + 0x200, 0, 0x200);
        if (on)
            for (i = 0; i < 4 * RULE_ROWS; ++i) {
                (pm + 0x200 + PM_TOP)[i] = 0xFF;  /* player 0: 8 columns */
                (pm + 0x280 + PM_TOP)[i] = 0xF0;  /* player 1: the other 4 */
            }
    }
    field = on;
    memcpy(HPOSP0, hpos[on], 4);
    memset(PCOLR0, on ? PM_COLOR : SNOW_COLOR, 4);
}

static void at(unsigned char r, unsigned char c)
{
    cur = row[r] + c;
}

static void put(unsigned char ch)
{
    if (ch < 0x60)                      /* lowercase ($60-$7F) is the same in both */
        ch -= 0x20;
    *cur++ = ch | inv;
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
    memset(row[r], inv, 40);
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
        put('\\');
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

static void syn_letters(unsigned char mask)
{
    for (i = 0; i < NSYN; ++i)
        if (mask >> i & 1) {
            inv = INV;
            put(syn_letter[i]);
            inv = 0;
            put(' ');
        }
}

#if !FANCY_TITLE
/* ---------------------------------------------------------------- title */

void display_title_screen(void)
{
    const unsigned char *p;
    setup();
    panel(0);
    memset(row[0], 0, 40 * 24);
    for (p = title_art; *p != 0xFF; p += 3 + p[2])
        memcpy(row[*p] + p[1], p + 3, p[2]);
    at(TITLE_END + 1, 4);
    text("REGION  ");                   /* from the server, once it answers */
    textn(region, region_len);
    at(TITLE_END + 2, 4);
    text("STATUS  ");                   /* what joining would do, from the server */
    if (status_len)
        textn(status_text, status_len - 1);
    else if (status_code == NO_CARRIER)
        text("NO CARRIER");
    at(TITLE_END + 3, 4);
    text("HANDLE  ");
    textn(edit, edit_len);
    inv = INV;
    put(' ');                           /* cursor */
    inv = 0;
    spaces(8 - edit_len);
    at(TITLE_END + 5, 4);
    inv = INV;
    text("OPTION");                     /* the OPTION key flips it */
    inv = 0;
    text("  MOVE HINTS ");
    inv = INV;
    text(hints ? "ON" : "OFF");
    inv = 0;
    put(' ');
    at(TITLE_END + 6, 4);
    inv = INV;
    text("HELP");                       /* the HELP key: instruction pages */
    inv = 0;
    text("    INSTRUCTIONS");
    at(TITLE_END + 8, 4);
    text(refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN"
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS ALREADY PLAYING" : "TYPE A HANDLE, THEN RETURN");
}
#endif

#if !FANCY_PLAYFIELD
/* ---------------------------------------------------------------- playfield */

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
    unsigned char r, c = node;
    for (r = 0; c >= RULE_COLS; ++r)
        c -= RULE_COLS;
    at(ROW_BOARD + r, COL_BOARD + c);
    v = board[node];
    if (v == CELL_EMPTY) {
        v = target(node);
        v = (v ? v : GLYPH_EMPTY) - 0x20;
    }
    else if (v == CELL_SPIKE)
        v = GLYPH_SPIKE - 0x20;
    else
        v = (syn_letter[v - 2] - 0x20) | INV;
    if (node == last_node) {
        v ^= INV;
        at(ROW_BOARD + 7 + 1, COL_PANEL);
        text("LAST: ");
        node_name(node);
        at(ROW_BOARD + r, COL_BOARD + c);
    }
    *cur = v;
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
        inv = INV;
        put(syn_letter[j]);
        inv = 0;
        n = hive_size[j];               /* then its size, next to it */
        if (n) {
            inv = n >= RULE_SAFESIZE ? INV : 0;     /* Black ICE */
            number(n, 3, 0);
            inv = 0;
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
    inv = seat == turn ? INV : 0;
    textn((const unsigned char *)seat_name[seat], 8);
    inv = 0;
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
    memmove(row[ROW_LOG], row[ROW_LOG + 1], 40 * (LOG_ROWS - 1));
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
            inv = v & INV;              /* can't run now: shown inverse */
            node_name(v & 0x7F);
            inv = 0;
        }
    }
}

static void ask(const char *s)
{
    inv = INV;
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(complaint ? complaint : s);
}

void display_prompt(void)
{
    if (pr_kind == 'P' || (pr_kind == 0 && turn != NONE))
        kit_line();
    else if (pr_kind != 'S' && pr_kind != 0)
        clear_row(ROW_INFO);            /* in the lobby row 22 keeps the last result */
    at(ROW_INFO, 1);
    switch (pr_kind) {
    case 0:
        if (turn < SEATS && turn != me) {
            inv = INV;
            clear_row(ROW_ASK);
            at(ROW_ASK, 1);
            text("WAITING FOR ");
            textn((const unsigned char *)seat_name[turn], 8);
            inv = 0;
        } else {
            clear_row(ROW_ASK);
        }
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
        text("BUY UP TO 3 " T_COIN ": ");
        textn(edit, edit_len);
        break;
    }
    if (pr_kind == 'B')
        ask(step ? T_NULLROUTE " AND END THE GAME? Y/N"
                 : "LETTERS, DELETE, RETURN TO BUY");
    inv = 0;
}

void display_clock(unsigned char secs)
{
    if (!pr_kind)
        return;
    inv = INV;
    at(ROW_ASK, 37);
    if (secs)
        number(secs, 3, 0);
    else
        spaces(3);
    inv = 0;
}

void display_message(const char *s)
{
    setup();
    inv = INV;
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(s);
    inv = 0;
}

void display_game_over(void)
{
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
}

void display_link(void)
{
    if (!field)                         /* still on the title screen */
        return;
    inv = INV;
    at(0, COL_BOARD);                   /* over the grid */
    textn(region, region_len);
    put(':');
    text(link_up ? "CONNECTED" : "NO SIGNAL");
    inv = 0;
}

void display_playfield(void)
{
    setup();
    panel(1);
    memset(row[0], 0, 40 * 24);
    inv = INV;
    clear_row(0);
    at(0, 39 - (sizeof TH_TITLE - 1));
    text(TH_TITLE);
    inv = 0;
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
        inv = INV;
        put(syn_letter[j]);
        inv = 0;
    }
    for (i = 0; i < SEATS; ++i)
        display_seat(i);
    display_prompt();
}
#endif
