/* The desktop's text display: the MS-DOS client's 80-column screen
 * (src/msdos/display_text.c), its layout and its colour rules, with the
 * circuit-board art either side, and the title's logo with each column doubled
 * over panels cleared round it, as there. Cells hold code page 437 characters and a
 * role (desk.h), not a colour: the window's colour theme gives each role its
 * colours, so the drawing code here is the DOS code with BIOS attributes
 * swapped for roles:
 *
 *  normal   the Grid's empty nodes (.), lone spikes (+) and move hints (1-6),
 *           the panel, the seats, the log; rows 22-23 while others play.
 *           Under the Grid's nodes and in the seats' rows, the panel's colours
 *  reverse  the title bar, Black ICE sizes, whose turn it is, rows 22-23
 *           while this seat has a decision to make (the bar and the question
 *           have roles of their own); a refusal at the foot of the title screen
 *  hive     a hive's letter: its syndicate's colours
 *  flash    the last move, lit and unlit by turns for FLASH_MS
 *
 * Every public function holds the screen's lock while it draws, so the window
 * never shows half a row. */
#include <string.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include "../../client/src/config.h"
#include "../../client/src/platform.h"
#include "../../client/src/display.h"
#include "../../client/src/common/proto.h"
#include "../../client/src/common/proto_tab.h"
#include "../../client/src/common/game.h"
#include "desk.h"

typedef struct cell cell;

#define NORMAL  0                       /* drawing modes, as the DOS display's attributes */
#define INV     1
#define FLASH   2
#define HIVE    3
#define FLIP    (NORMAL ^ INV)          /* mode ^ FLIP: normal <-> reverse */
#define YEN     0x9D                    /* code page 437 */

#define FLASH_MS    5000
#define BLINK_MS    250                 /* lit and unlit by turns, twice a second */

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
#define ROW_KEYS    18                  /* F2, F3, F1, F11 */
#define COL_KEYS    18                  /* what each key does */
#define COL_HINTS   (COL_KEYS + 11)     /* ON/OFF after "MOVE HINTS" */

#include "../../client/src/msdos/title_art.h"   /* title_art[], TITLE_END */
#include "../../client/src/msdos/margins.h"     /* margins[], MARGIN_W */
#if TITLE_END >= ROW_REGION - 1
#error "the title logo runs into the title's lines: at most rows 0-13"
#endif

cell scr[SCR_ROWS][SCR_COLS];
static cell (*screen)[SCR_COLS] = scr;  /* where drawing goes: scr, or `shadow` under the instructions */
static cell *cur;
static unsigned char cr, cc;            /* the row and column put() writes next */
static unsigned char helping;           /* the instructions are up */
static unsigned char mode = NORMAL, i, j, n, v;
static unsigned char field;             /* the playfield is up: the title bar may draw */
static unsigned char bottom = NORMAL;   /* rows 22-23: INV while this seat decides */
static SDL_Mutex *lock;

void scr_lock(void)
{
    if (!lock)
        lock = SDL_CreateMutex();
    SDL_LockMutex(lock);
}

void scr_unlock(void)
{
    SDL_UnlockMutex(lock);
}

/* ---------------------------------------------------------------- drawing */

static unsigned char role(unsigned char m, unsigned char ch, unsigned char r, unsigned char c)
{
    if (m == HIVE) {
        unsigned char k;
        for (k = 0; k < NSYN - 1 && syn_letter[k] != ch; ++k)
            ;
        return R_HIVE + k;
    }
    if (m == FLASH)
        return R_LAST;
    if (m == INV)
        return r == 0 && field ? R_BAR : r >= ROW_INFO && field ? R_ASK : R_INV;
    return field && ((r >= ROW_BOARD && r < ROW_BOARD + RULE_ROWS && c >= COL_BOARD && c < COL_BOARD + RULE_COLS)
                     || (r >= ROW_SEATS && r < ROW_SEATS + SEATS)) ? R_PANEL : R_TEXT;
}

static void at(unsigned char r, unsigned char c)
{
    cur = &screen[r][GAME_LEFT + c];
    cr = r;
    cc = c;
}

static void put(unsigned char ch)
{
    if (ch == '\\')
        ch = YEN;
    cur->ch = ch;
    cur->role = role(mode, ch, cr, cc++);
    ++cur;
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

/* `title`: the title screen, whose panels are cleared round the logo */
static void clear_screen(unsigned char title)
{
    unsigned char r, c;
    const unsigned char *m;
    mode = NORMAL;
    for (r = 0; r < SCR_ROWS; ++r)
        for (c = 0; c < SCR_COLS; ++c) {
            screen[r][c].ch = ' ';
            screen[r][c].role = R_TEXT;
        }
    for (r = 0; r < SCR_ROWS; ++r)      /* the side panels */
        for (c = 0; c < MARGIN_W; ++c) {
            m = title && r < TITLE80_ROWS ? title_margins[r] : margins[r];
            screen[r][c].ch = m[c];
            screen[r][c].role = R_ART;
            screen[r][SCR_COLS - MARGIN_W + c].ch = m[MARGIN_W + c];
            screen[r][SCR_COLS - MARGIN_W + c].role = R_ART;
        }
    for (r = 0; r < SCR_ROWS; ++r)      /* the game's 40 columns, cell by cell for the panels' colour */
        clear_row(r);
}

/* Right-aligned decimal in `width` columns. `money` (units of 100) appends "00"
 * and puts the currency sign right before the digits, inside `width`. */
static void number(unsigned value, unsigned char width, unsigned char money)
{
    char digits[12];
    unsigned char len;
    if (money && value)
        len = (unsigned char)SDL_snprintf(digits, sizeof digits, "%u00", value);
    else
        len = (unsigned char)SDL_snprintf(digits, sizeof digits, "%u", value);
    if (money)
        ++len;                          /* the sign */
    if (width > len)
        spaces(width - len);
    if (money) {
        put(YEN);
        --len;
    }
    textn((const unsigned char *)digits, len);
}

static void node_name(unsigned char node)
{
    put('A' + node % RULE_COLS);
    put('0' + node / RULE_COLS);
}

/* The syndicate letters in `mask`, each in its hive's colours. */
static void syn_letters(unsigned char mask)
{
    for (i = 0; i < NSYN; ++i)
        if (mask >> i & 1) {
            v = mode;
            mode = HIVE;
            put(syn_letter[i]);
            mode = v;
            put(' ');
        }
}

/* ---------------------------------------------------------------- title */

static void title_update(void);

void display_title_screen(void)
{
    const unsigned char *p;
    scr_lock();
    field = 0;
    clear_screen(1);
    for (p = title_art; *p != 0xFF; p += 3 + p[2]) {
        cur = &screen[p[0]][2 * p[1] + TITLE80_SHIFT];     /* each column doubled, as MS-DOS's 80 columns */
        cr = p[0];
        for (n = 0; n < p[2]; ++n) {
            v = p[3 + n];
            put(v);
            /* a piece with an arm to the right goes on with a line; the hyphen's half block, and the cell
             * before the hyphen, with a half block (the hyphen centred between the letters' lines) */
            put(strchr("\xC4\xC3\xDA\xC0\xC2\xC1\xC5", v) ? 0xC4
                : v == 0xDF || (n + 1 < p[2] && p[4 + n] == 0xDF) ? 0xDF : ' ');
        }
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
    at(ROW_KEYS + 3, 4);
    text("F11");
    mode = NORMAL;
    at(ROW_KEYS, COL_KEYS);
    text("MOVE HINTS");
    at(ROW_KEYS + 1, COL_KEYS);
    text("TOGGLE SOUND");
    at(ROW_KEYS + 2, COL_KEYS);
    text("INSTRUCTIONS, ANY TIME");
    at(ROW_KEYS + 3, COL_KEYS);
    text("FULL SCREEN");
    if (refused)
        mode = INV;                     /* a refusal stands out at the foot */
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN"
         : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN ENTER. ESC QUITS");
    mode = NORMAL;
    title_update();
    scr_unlock();
}

/* Each field is padded to its widest, so drawing over the last one leaves nothing behind. */
static void title_update(void)
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

void display_title_update(void)
{
    scr_lock();
    title_update();
    scr_unlock();
}

/* ---------------------------------------------------------------- playfield */

/* The last move flashes from when it is first drawn until FLASH_MS have
 * passed (display_idle); then `done` holds it, and it shows as usual. */
static unsigned char blink = NONE, done = NONE, lit;
static Uint64 blink_t;

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

static void node(unsigned char node)
{
    unsigned char r = node / RULE_COLS, c = node % RULE_COLS, m = NORMAL;
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
                blink_t = SDL_GetTicks();
                lit = 0;
            }
            if (!lit)
                m = FLASH;
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    at(ROW_BOARD + r, COL_BOARD + c);
    mode = m;
    put(v);
    mode = NORMAL;
}

void display_node(unsigned char n)
{
    scr_lock();
    node(n);
    scr_unlock();
}

void display_idle(void)
{
    Uint64 t;
    if (blink == NONE)
        return;
    scr_lock();
    t = SDL_GetTicks() - blink_t;
    if (blink != last_node)
        blink = NONE;                   /* a newer move took over, or a new game */
    else if (t >= FLASH_MS) {
        done = blink;
        blink = NONE;
        node(done);
    } else if ((t / BLINK_MS & 1) != lit) {
        lit ^= 1;
        node(blink);
    }
    scr_unlock();
}

static void hives(void)
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
        n = (unsigned char)strlen(syn_name[j]);
        text(syn_name[j]);
        spaces(40 - COL_NAME - n);
    }
}

void display_hives(void)
{
    scr_lock();
    hives();
    scr_unlock();
}

static void seat(unsigned char seat)
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

void display_seat(unsigned char s)
{
    scr_lock();
    seat(s);
    scr_unlock();
}

void display_log(void)
{
    scr_lock();
    for (i = 0; i < LOG_ROWS - 1; ++i)
        for (j = 0; j < 40; ++j)
            screen[ROW_LOG + i][GAME_LEFT + j] = screen[ROW_LOG + i + 1][GAME_LEFT + j];
    clear_row(ROW_LOG + LOG_ROWS - 1);
    at(ROW_LOG + LOG_ROWS - 1, 1);
    n = msg[1];
    if (n > 39)
        n = 39;
    textn(msg + 2, n);
    scr_unlock();
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
    for (i = 0; i < 40; ++i) {
        cell *k = &screen[ROW_INFO][GAME_LEFT + i];
        k->role = role(bottom, k->ch, ROW_INFO, i);
    }
}

static void prompt(void)
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

void display_prompt(void)
{
    scr_lock();
    prompt();
    scr_unlock();
}

void display_clock(unsigned char secs)
{
    if (!pr_kind)
        return;
    scr_lock();
    mode = bottom;
    at(ROW_ASK, 37);
    if (secs)
        number(secs, 3, 0);
    else
        spaces(3);
    mode = NORMAL;
    scr_unlock();
}

void display_message(const char *s)
{
    scr_lock();
    mode = INV;
    clear_row(ROW_ASK);
    at(ROW_ASK, 1);
    text(s);
    mode = NORMAL;
    scr_unlock();
}

void display_game_over(void)
{
    scr_lock();
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
    scr_unlock();
}

static void link_bar(void)
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

void display_link(void)
{
    scr_lock();
    link_bar();
    scr_unlock();
}

void display_playfield(void)
{
    scr_lock();
    field = 1;
    clear_screen(0);
    mode = INV;
    clear_row(0);
    at(0, 39 - (sizeof TH_TITLE - 1));
    text(TH_TITLE);
    mode = NORMAL;
    link_bar();
    at(1, COL_BOARD);
    for (j = 0; j < RULE_COLS; ++j)
        put('A' + j);
    for (j = 0; j < RULE_ROWS; ++j) {
        at(ROW_BOARD + j, 0);
        put('0' + j);
    }
    for (j = 0; j < NODES; ++j)
        node(j);
    hives();
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
        seat(i);
    bottom = NORMAL;                    /* rows 22-23 are blank: no need to recolor */
    prompt();
    scr_unlock();
}

/* ---------------------------------------------------------------- instructions
 * The MS-DOS client's pages, SYNACK.HLP (tools/help_msdos.py), from the data
 * folder: page n is the 1920 bytes from (n-1) * 1920, its 24 rows of 40 cells,
 * character and DOS attribute (normal 07, reverse 70, bright 0F). While they're
 * up the game keeps drawing into `shadow`, and closing copies it back. '>' past
 * the last finds no page there and stays put. They open again where they were
 * left. */
#define HELP_FILE   "SYNACK.HLP"
#define PAGE_CELLS  (24 * 40)
#define PAGE_BYTES  (PAGE_CELLS * 2)

static cell shadow[SCR_ROWS][SCR_COLS];
static unsigned char page = 1;
static char help_path[1024] = HELP_FILE;

void display_set_data_dir(const char *dir)
{
    SDL_snprintf(help_path, sizeof help_path, "%s%s", dir, HELP_FILE);
}

/* Page `p` onto the window's screen; 0 if there is no such page. */
static unsigned char help_page(unsigned char p)
{
    static const char none[] = "NO INSTRUCTIONS: " HELP_FILE, back[] = "ESC RETURNS TO THE GAME";
    unsigned char buf[PAGE_BYTES];
    unsigned k;
    size_t got = 0;
    FILE *f = fopen(help_path, "rb");
    if (f) {
        if (fseek(f, (long)(p - 1) * PAGE_BYTES, SEEK_SET) == 0)
            got = fread(buf, 1, PAGE_BYTES, f);
        fclose(f);
    }
    if (got != PAGE_BYTES) {
        if (p > 1)
            return 0;                   /* past the last page */
        for (k = 0; k < PAGE_CELLS; ++k) {              /* no pages at all: say so */
            buf[2 * k] = ' ';
            buf[2 * k + 1] = 0x07;
        }
        for (k = 0; none[k]; ++k)
            buf[2 * (11 * 40 + 6 + k)] = none[k];
        for (k = 0; back[k]; ++k)
            buf[2 * (13 * 40 + 6 + k)] = back[k];
    }
    for (k = 0; k < PAGE_CELLS; ++k) {
        cell *c = &scr[k / 40][GAME_LEFT + k % 40];
        v = buf[2 * k + 1];
        c->ch = buf[2 * k];
        c->role = v == 0x70 ? R_INV : v == 0x0F ? R_BRIGHT : R_TEXT;
    }
    for (k = 0; k < 40; ++k) {          /* row 24, under the page */
        scr[24][GAME_LEFT + k].ch = ' ';
        scr[24][GAME_LEFT + k].role = R_TEXT;
    }
    return 1;
}

void help_open(void)
{
    scr_lock();
    memcpy(shadow, scr, sizeof scr);    /* the game goes on drawing here */
    screen = shadow;
    helping = 1;
    help_page(page);
    scr_unlock();
}

static void help_close(void)
{
    memcpy(scr, shadow, sizeof scr);
    screen = scr;
    helping = 0;
}

unsigned char help_key(unsigned char k)
{
    unsigned char open = 1;
    scr_lock();
    if (!helping)                       /* closed by display_reset(): main.c catches up */
        open = 0;
    else if (k == '>') {
        if (help_page(page + 1))
            ++page;
    } else if (k == '<') {
        if (page > 1 && help_page(page - 1))
            --page;
    } else if (k == K_ESC || k == K_RETURN || k == K_HELP) {
        help_close();
        open = 0;
    }
    scr_unlock();
    return open;
}

/* Before the game thread starts again from the title (desk.c): no instructions
 * up, and if they were, a key so main.c sees them closed. */
void display_reset(void)
{
    scr_lock();
    if (helping) {
        help_close();
        key_push(K_NONE);
    }
    field = 0;
    blink = done = NONE;
    scr_unlock();
}
