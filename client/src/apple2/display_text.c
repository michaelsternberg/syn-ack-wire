/* The text-only display for the Apple II (apple2_text): the 40x24 text screen,
 * page 1, written straight to screen memory. The layout is the Atari's
 * (src/atari/display_text.c has the row map); the Apple has no colour in text
 * mode, so the primary character set's three modes carry it all:
 *
 *  normal   the Grid's empty nodes (.), lone spikes (+) and move hints (1-6),
 *           the panel, the seats, the log; rows 22-23 while others play
 *  inverse  the title bar, hives (letters), Black ICE sizes, the syndicate
 *           letters, whose turn it is; rows 22-23 while this seat has a
 *           decision to make (inside them, what normally shows inverse shows
 *           normal); a refusal at the foot of the title screen
 *  flash    the last move, for FLASH_SECS, then it shows as usual
 *
 * Screen rows are interleaved in memory; row[] has their addresses. Only the
 * 40 bytes of each row are written: the 8 left over in each 128 (the "screen
 * holes") belong to the peripheral cards' firmware, the FujiNet's included.
 *
 * Text arrives as ASCII $20-$5F. There is no yen glyph: money and "\" from the
 * server (the Atari's yen sign) show as Y. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"

#ifdef PC_SCREEN                        /* a PC build for the manual's captures (test/pc_harness.c) */
extern unsigned char PC_SCREEN[1024], pc_switch;
#define PAGE1   PC_SCREEN
#define SW(a)   (*(volatile unsigned char *)&pc_switch)
#else
#define PAGE1   ((unsigned char *)0x400)
#define SW(a)   (*(volatile unsigned char *)(a))
#endif
#define CLR80STORE  0xC000              /* write: PAGE2 then shows text page 2, not aux memory */
#define CLR80VID    0xC00C              /* write: 40 columns */
#define CLRALTCHAR  0xC00E              /* write: the primary set, which has flash */
#define TXTSET      0xC051              /* text mode */
#define TXTPAGE1    0xC054

#define NORMAL  0x80                    /* screen code = (ASCII & $3F) | mode */
#define INV     0x00
#define FLASH   0x40

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
#define ROW_KEYS    19                  /* OPEN-APPLE, CLOSED-APPLE, ? */
#define COL_KEYS    18                  /* what each key does */
#define COL_HINTS   (COL_KEYS + 11)     /* ON/OFF after "MOVE HINTS" */

#include "title_art.h"                  /* title_art[], TITLE_END */
#if TITLE_END >= ROW_REGION - 1
#error "the title logo runs into the title's lines: at most rows 0-13"
#endif

#define R(n) (PAGE1 + ((n) & 7) * 0x80 + ((n) >> 3) * 40)
static unsigned char *const row[24] = {
    R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),
    R(8),  R(9),  R(10), R(11), R(12), R(13), R(14), R(15),
    R(16), R(17), R(18), R(19), R(20), R(21), R(22), R(23)
};

static unsigned char *cur;
static unsigned char mode = NORMAL, ready, i, j, n, v;
static unsigned char field;             /* the playfield is up: the title bar may draw */
static unsigned char bottom = NORMAL;   /* rows 22-23: INV while this seat decides */

/* ---------------------------------------------------------------- drawing */

static void setup(void)
{
    if (ready)
        return;
    ready = 1;
    SW(TXTSET);
    SW(TXTPAGE1);
    SW(CLR80VID) = 0;
    SW(CLR80STORE) = 0;
    SW(CLRALTCHAR) = 0;
}

static void at(unsigned char r, unsigned char c)
{
    cur = row[r] + c;
}

static void put(unsigned char ch)
{
    if (ch == '\\')
        ch = 'Y';
    *cur++ = (ch & 0x3F) | mode;
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
    memset(row[r], ' ' | mode, 40);     /* ' ' & $3F is ' ' */
}

static void clear_screen(void)
{
    for (i = 0; i < 24; ++i)
        memset(row[i], ' ' | NORMAL, 40);
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
        put('Y');
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
            mode ^= NORMAL;
            put(syn_letter[i]);
            mode ^= NORMAL;
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
        memcpy(row[*p] + p[1], p + 3, p[2]);
    at(ROW_REGION, 4);
    text("REGION");                     /* from the server, once it answers */
    at(ROW_REGION + 1, 4);
    text("STATUS");                     /* what joining would do, from the server */
    at(ROW_REGION + 2, 4);
    text("HANDLE");
    mode = INV;
    at(ROW_KEYS, 4);
    text("OPEN-APPLE");                 /* the key flips it */
    at(ROW_KEYS + 1, 4);
    text("CLOSED-APPLE");
    at(ROW_KEYS + 2, 4);
    put('?');
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
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN RETURN");
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

/* The last move flashes from when it is first drawn until FLASH_SECS have
 * passed (display_idle); then `done` holds it, and it shows as usual. */
static unsigned char blink = NONE, blink_t, done = NONE;

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
        m = INV;
    }
    if (node == last_node) {
        at(ROW_BOARD + 7 + 1, COL_PANEL);
        text("LAST: ");
        node_name(node);
        if (node != done) {
            m = FLASH;
            if (blink != node) {
                blink = node;
                blink_t = ticks();
            }
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    row[ROW_BOARD + r][COL_BOARD + c] = (v & 0x3F) | m;
}

void display_idle(void)
{
    if (blink == NONE)
        return;
    if (blink != last_node)
        blink = NONE;                   /* a newer move took over, or a new game */
    else if ((unsigned char)(ticks() - blink_t) >= FLASH_SECS) {
        done = blink;
        blink = NONE;
        display_node(done);
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
    for (i = 0; i < LOG_ROWS - 1; ++i)  /* rows aren't contiguous: one at a time */
        memcpy(row[ROW_LOG + i], row[ROW_LOG + i + 1], 40);
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
                mode ^= NORMAL;
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
    cur = row[ROW_INFO];
    for (i = 0; i < 40; ++i, ++cur)
        *cur = (*cur & 0x3F) | bottom;
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
            at(ROW_ASK, 33);            /* the instructions, while there's time */
            text("? HELP");
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
                 : "LETTERS, DELETE, RETURN TO BUY");
    mode = NORMAL;
}

void display_clock(unsigned char secs)
{
    if (!pr_kind)
        return;
    mode = bottom;
    at(ROW_ASK, 37);
    if (secs)
        number(secs, 3, 0);
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
        mode = INV;
        put(syn_letter[j]);
        mode = NORMAL;
    }
    for (i = 0; i < SEATS; ++i)
        display_seat(i);
    bottom = NORMAL;                    /* rows 22-23 are blank: no need to recolor */
    display_prompt();
}
#endif
