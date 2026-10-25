/* The text-only display for MS-DOS: the 40x24 layout in the middle of the
 * 80x25 text screen (columns 20-59), written straight to video memory. The
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
 * The 20 columns either side of the game hold circuit-board art (margins.txt):
 * decoration only, drawn with every full redraw, dark grey on a colour adapter
 * so it stays behind the game; normal on a monochrome one, where dark grey
 * would not show at all. */
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
#define FAR __far
#define FMEMCPY _fmemcpy                /* video memory is in another segment */
#endif
typedef unsigned short FAR cell;

#define NORMAL  0x07                    /* attribute: light grey on black */
#define INV     0x70                    /* black on light grey */
#define FLASH   0xF0                    /* the same, blinking */
#define FLIP    (NORMAL ^ INV)          /* mode ^ FLIP: normal <-> reverse */
#define YEN     0x9D                    /* code page 437 */

#define LEFT    20                      /* the 40 columns start here, in 80 */

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
#define ROW_KEYS    19                  /* F2, F3, F1 */
#define COL_KEYS    18                  /* what each key does */
#define COL_HINTS   (COL_KEYS + 11)     /* ON/OFF after "MOVE HINTS" */

#include "title_art.h"                  /* title_art[], TITLE_END */
#include "margins.h"                    /* margins[], MARGIN_W */
#define ART_COLOUR  0x08                /* the panels: dark grey */
#if TITLE_END >= ROW_REGION - 1
#error "the title logo runs into the title's lines: at most rows 0-13"
#endif

static cell *screen;                    /* row 0, column 0 of the 80x25 screen */
static cell *cur;
static unsigned char mode = NORMAL, ready, i, j, n, v;
static unsigned char art = ART_COLOUR;  /* the panels' attribute */
static unsigned char field;             /* the playfield is up: the title bar may draw */
static unsigned char bottom = NORMAL;   /* rows 22-23: INV while this seat decides */

#define ROW(r)  (screen + (r) * 80 + LEFT)

/* ---------------------------------------------------------------- drawing */

/* Text mode, once: a monochrome adapter (BIOS mode 7) stays as it is, at B000;
 * anything else is set to 80x25 colour text (mode 3), at B800. The cursor is
 * hidden: the game draws its own. */
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
    if (r.h.al == 7) {
        screen = (cell *)0xB0000000L;
        art = NORMAL;
    } else {
        if (r.h.al != 2 && r.h.al != 3) {
            r.x.ax = 0x0003;
            int86(0x10, &r, &r);
        }
        screen = (cell *)0xB8000000L;
    }
    r.h.ah = 0x01;                      /* cursor shape: off */
    r.x.cx = 0x2000;
    int86(0x10, &r, &r);
#endif
}

static void at(unsigned char r, unsigned char c)
{
    cur = ROW(r) + c;
}

static void put(unsigned char ch)
{
    if (ch == '\\')
        ch = YEN;
    *cur++ = mode << 8 | ch;
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
    for (k = 0; k < 25 * 80; ++k)
        screen[k] = NORMAL << 8 | ' ';
    for (k = 0; k < 25; ++k)            /* the side panels */
        for (j = 0; j < MARGIN_W; ++j) {
            screen[k * 80 + j] = art << 8 | margins[k][j];
            screen[k * 80 + 80 - MARGIN_W + j] = art << 8 | margins[k][MARGIN_W + j];
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
            mode ^= FLIP;
            put(syn_letter[i]);
            mode ^= FLIP;
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
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN ENTER");
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
    ROW(ROW_BOARD + r)[COL_BOARD + c] = m << 8 | v;
}

void display_idle(void)
{
    if (blink == NONE)
        return;
    if (blink != last_node)
        blink = NONE;                   /* a newer move took over, or a new game */
    else if ((unsigned char)(ticks() - blink_t) >= FLASH_SECS * 18) {
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
        *cur = bottom << 8 | (*cur & 0xFF);
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
    for (k = 0; k < PAGE_CELLS; ++k)
        video[k / 40 * 80 + LEFT + k % 40] = buf[k];
    return 1;
}

void help_open(void)
{
    setup();
    video = screen;
    FMEMCPY(shadow, video, sizeof shadow);      /* the game goes on drawing here */
    screen = shadow;
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
        FMEMCPY(video, shadow, sizeof shadow);
        screen = video;
        return 0;
    }
    return 1;
}
