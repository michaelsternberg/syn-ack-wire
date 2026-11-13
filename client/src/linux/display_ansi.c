/* The linux client's screens: all of display.h, drawn into nl_cells (screen.h) in colour, in the layout of layout.h
 * (80x24, from the reference screenshot). The game state is read from game.h, as every display does. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"
#include "screen.h"
#include "layout.h"
#include "title_art.h"
#include "help_tab.h"

#define YEN 0xA5
#define MIDDLE_DOT 0xB7
#define MARKER 0x25B6
#define FLASH_SECS 5

static unsigned char blink = NONE, blink_t, lit, done = NONE;   /* the last move's blink: which node, from when, its phase */

/* ---------------------------------------------------------------- helpers */

static void number(unsigned char r, unsigned char c, unsigned long v, unsigned char fg, unsigned char bg)
{
    char buf[12];
    unsigned char n = 0;
    do {
        buf[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        nl_put(r, c++, (unsigned char)buf[--n], fg, bg, 0);
}

static void number_ending_strip(unsigned char r, unsigned char last, unsigned long v);
static unsigned char digits_of(unsigned long v)
{
    unsigned char n = 1;
    while (v >= 10) {
        v /= 10;
        ++n;
    }
    return n;
}

/* `v` right-aligned so that it ends in column `last` */
static void number_ending(unsigned char r, unsigned char last, unsigned long v, unsigned char fg, unsigned char bg)
{
    number(r, (unsigned char)(last + 1 - digits_of(v)), v, fg, bg);
}

static void number_ending_strip(unsigned char r, unsigned char last, unsigned long v)
{
    number_ending(r, last, v, C_WHITE, C_STRIP);
}

static void node_rc(unsigned char node, unsigned char *r, unsigned char *c)
{
    *r = (unsigned char)(node / RULE_COLS);
    *c = (unsigned char)(node % RULE_COLS);
}

static unsigned char hint_digit(unsigned char node)
{
    unsigned char n;
    if (!hints)
        return 0;
    for (n = 0; n < RULE_KITSIZE; ++n)
        if (kit[n] == node)             /* bit 7 set = can't run now: never equal */
            return (unsigned char)('1' + n);
    return 0;
}

/* ---------------------------------------------------------------- the title bar */

void display_link(void)
{
    unsigned char c = 17, i;
    nl_fill(ROW_BAR, 17, 31, C_WHITE, C_BAR);
    if (region_len) {
        for (i = 0; i < region_len; ++i)
            nl_put(ROW_BAR, c++, region[i], C_WHITE, C_BAR, NL_BOLD);
    } else {
        for (i = 0; TH_REGION[i]; ++i)
            nl_put(ROW_BAR, c++, (unsigned char)TH_REGION[i], C_WHITE, C_BAR, NL_BOLD);
    }
    nl_put(ROW_BAR, c++, ':', C_DIM, C_BAR, 0);
    nl_text(ROW_BAR, c, link_up ? "CONNECTED" : "NO SIGNAL", link_up ? C_GREEN : C_RED, C_BAR);
}

static void title_bar(void)
{
    unsigned char c = 1, i, second = 0;
    nl_fill(ROW_BAR, 0, NL_COLS, C_WHITE, C_BAR);
    for (i = 0; TH_TITLE[i]; ++i) {     /* the first word cyan, the rest magenta */
        if (TH_TITLE[i] == ' ')
            second = 1;
        nl_put(ROW_BAR, c++, (unsigned char)TH_TITLE[i], second ? C_MAGENTA : C_CYAN, C_BAR, NL_BOLD);
    }
    display_link();
    nl_text(ROW_BAR, 49, "?", C_CYAN, C_BAR);
    nl_text(ROW_BAR, 51, "HELP", C_DIM, C_BAR);
    nl_text(ROW_BAR, 57, "TAB", C_CYAN, C_BAR);
    nl_text(ROW_BAR, 61, "HINTS", C_DIM, C_BAR);
    nl_text(ROW_BAR, 68, "^S", C_CYAN, C_BAR);
    nl_text(ROW_BAR, 71, "SOUND", C_DIM, C_BAR);
}

/* ---------------------------------------------------------------- the Grid */

void display_node(unsigned char node)
{
    unsigned char r, c, col, v = board[node], k, fg = C_DIM, bg = C_EMPTY, attr = 0, hint;
    unsigned int mid;
    node_rc(node, &r, &c);
    col = (unsigned char)(COL_GRID + CELL_W * c);
    if (v == CELL_EMPTY) {
        hint = hint_digit(node);
        if (hint) {
            mid = hint;
            fg = C_YELLOW;
            attr = NL_BOLD;
        } else {
            mid = MIDDLE_DOT;
        }
    } else if (v == CELL_SPIKE) {
        mid = '+';
        fg = C_WHITE;
        bg = C_GREY;
    } else {
        mid = (unsigned char)syn_letter[v - 2];
        fg = C_BLACK;
        bg = (unsigned char)(C_SYN0 + v - 2);
        attr = NL_BOLD;
    }
    if (node == last_node) {
        nl_fill(ROW_LAST, 0, NL_COLS, C_WHITE, C_BG);
        nl_text(ROW_LAST, COL_GRID, "LAST MOVE ", C_DIM, C_BG);
        nl_put(ROW_LAST, COL_GRID + 10, (unsigned char)('A' + c), C_WHITE, C_BG, NL_BOLD);
        nl_put(ROW_LAST, COL_GRID + 11, (unsigned char)('0' + r), C_WHITE, C_BG, NL_BOLD);
        if (node != done) {
            if (blink != node) {
                blink = node;
                blink_t = ticks();
                lit = 0;
            }
            if (lit) {
                fg = C_BLACK;
                bg = C_WHITE;
            }
        }
    } else if (node == done) {
        done = NONE;                    /* no longer the last move: it may be again, next game */
    }
    for (k = 0; k < CELL_W; ++k)
        nl_put(r + ROW_GRID, (unsigned char)(col + k), k == 1 ? mid : ' ', fg, bg, k == 1 ? attr : 0);
}

void display_idle(void)
{
    unsigned char v;
    if (blink == NONE)
        return;
    if (blink != last_node) {
        blink = NONE;                   /* a newer move took over, or a new game */
    } else if ((unsigned char)(v = (unsigned char)(ticks() - blink_t)) >= FLASH_SECS * 50) {
        done = blink;
        blink = NONE;
        display_node(done);
    } else if ((v >> 4 & 1) != lit) {   /* lit and unlit by turns, about three times a second */
        lit ^= 1;
        display_node(blink);
    }
}

/* ---------------------------------------------------------------- the syndicates */

/* number with the yen sign and thousands separators, right-aligned to end at column `last` (units of 100) */
static void yen_ending(unsigned char r, unsigned char last, unsigned long units, unsigned char fg, unsigned char bg)
{
    char digs[12], out[16];
    unsigned char n = 0, o = 0, i, col;
    unsigned long v = units * 100UL;
    do {
        digs[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (i = 0; i < n; ++i) {
        out[o++] = digs[n - 1 - i];
        if ((n - 1 - i) % 3 == 0 && i != n - 1)
            out[o++] = ',';
    }
    col = (unsigned char)(last + 1 - o - 1);
    nl_put(r, col++, YEN, fg, bg, 0);
    for (i = 0; i < o; ++i)
        nl_put(r, col++, (unsigned char)out[i], fg, bg, 0);
}

/* while the buy prompt is open (pr: timeout, cash[2], flags, price[7], supply[7]) the bar's place shows each hive's
 * price, and the badge's its supply */
void display_hives(void)
{
    unsigned char k, r, fill, c, buying = pr_kind == 'B';
    const char *ice = strrchr(T_SAFE, ' ');
    ice = ice ? ice + 1 : T_SAFE;
    nl_fill(ROW_HEAD, COL_BAR, (unsigned char)(NL_COLS - COL_BAR), C_WHITE, C_BG);
    nl_text(ROW_HEAD, COL_BAR, buying ? "PRICE" : "SIZE", C_DIM, C_BG);
    nl_text(ROW_HEAD, COL_NAME, "SYNDICATE", C_DIM, C_BG);
    if (buying)
        nl_text(ROW_HEAD, COL_ICE - 1, "LEFT", C_DIM, C_BG);
    for (k = 0; k < NSYN; ++k) {
        r = (unsigned char)(ROW_GRID + k);
        nl_fill(r, COL_HIVE, (unsigned char)(NL_COLS - COL_HIVE), C_WHITE, C_BG);
        nl_put(r, COL_HIVE, ' ', C_BLACK, (unsigned char)(C_SYN0 + k), 0);
        nl_put(r, COL_HIVE + 1, (unsigned char)syn_letter[k], C_BLACK, (unsigned char)(C_SYN0 + k), NL_BOLD);
        nl_put(r, COL_HIVE + 2, ' ', C_BLACK, (unsigned char)(C_SYN0 + k), 0);
        if (hive_size[k])
            number_ending(r, COL_SIZE + 2, hive_size[k], C_WHITE, C_BG);
        else
            nl_put(r, COL_SIZE + 2, '-', C_DIM, C_BG, 0);
        nl_text(r, COL_NAME, syn_name[k], (unsigned char)(C_SYN0 + k), C_BG);
        if (buying) {
            if (pr[4 + k]) {
                yen_ending(r, COL_BAR + BAR_W - 1, pr[4 + k], C_YELLOW, C_BG);
                number_ending(r, COL_ICE + 2, pr[11 + k], C_WHITE, C_BG);
            }
            continue;
        }
        nl_fill(r, COL_BAR, BAR_W, C_WHITE, C_EMPTY);
        fill = (unsigned char)(hive_size[k] * BAR_W / SIZE_MAX);
        if (hive_size[k] && !fill)
            fill = 1;
        if (fill > BAR_W)               /* hives may outgrow the null-route size */
            fill = BAR_W;
        nl_fill(r, COL_BAR, fill, C_WHITE, (unsigned char)(C_SYN0 + k));
        if (hive_size[k] >= RULE_SAFESIZE)
            nl_text(r, COL_ICE, ice, C_WHITE, C_BLUE);
    }
    nl_fill(ROW_ICE, COL_HIVE, (unsigned char)(NL_COLS - COL_HIVE), C_WHITE, C_BG);
    nl_text(ROW_ICE, COL_HIVE, T_SAFE " AT ", C_DIM, C_BG);
    c = (unsigned char)(COL_HIVE + sizeof T_SAFE + 3);
    number(ROW_ICE, c, RULE_SAFESIZE, C_WHITE, C_BG);
    c = (unsigned char)(c + digits_of(RULE_SAFESIZE));
    nl_text(ROW_ICE, c, ", " T_NULLROUTE " AT ", C_DIM, C_BG);
    c = (unsigned char)(c + sizeof ", " T_NULLROUTE " AT " - 1);
    number(ROW_ICE, c, RULE_ENDSIZE, C_WHITE, C_BG);
}

/* ---------------------------------------------------------------- the seats */

/* the cash (units of 100) as the yen sign and digits, right-aligned in CASH_W: with thousands separators if they fit */
static void cash_at(unsigned char r, unsigned long units)
{
    char digs[12], out[16];
    unsigned char n = 0, o = 0, i, commas;
    unsigned long v = units * 100UL;
    unsigned char col;
    do {
        digs[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    commas = (unsigned char)((n - 1) / 3);
    if (1 + n + commas > CASH_W)
        commas = 0;
    for (i = 0; i < n; ++i) {
        out[o++] = digs[n - 1 - i];
        if (commas && (n - 1 - i) % 3 == 0 && i != n - 1)
            out[o++] = ',';
    }
    col = (unsigned char)(COL_CASH + CASH_W - 1 - o);
    nl_put(r, col++, YEN, C_YELLOW, C_BG, 0);
    for (i = 0; i < o; ++i)
        nl_put(r, col++, (unsigned char)out[i], C_YELLOW, C_BG, 0);
}

void display_seat(unsigned char s)
{
    unsigned char r, k, kind, c, n;
    if (s >= SEATS)
        return;
    r = (unsigned char)(ROW_SEATS + s);
    nl_fill(r, 0, NL_COLS, C_WHITE, C_BG);
    kind = seat_kind[s];
    if (kind == SEAT_EMPTY) {
        nl_put(r, COL_SEAT, '-', C_DIM, C_BG, 0);
        return;
    }
    if (s == turn)
        nl_put(r, COL_TURN, MARKER, C_CYAN, C_BG, NL_BOLD);
    for (c = 0; c < 8; ++c)
        nl_put(r, (unsigned char)(COL_SEAT + c), (unsigned char)seat_name[s][c], s == turn ? C_CYAN : C_WHITE, C_BG,
               s == turn ? NL_BOLD : 0);
    nl_put(r, COL_STATUS, kind == SEAT_AWAY ? '?' : kind == SEAT_BOT ? '*' : ' ', C_DIM, C_BG, 0);
    cash_at(r, cash[s]);
    for (k = 0; k < NSYN; ++k) {
        n = coins[s][k];
        c = (unsigned char)(COL_HOLD0 + HOLD_STEP * k + 2);
        if (n)
            number_ending(r, c, n, C_WHITE, C_BG);
        else
            nl_put(r, c, MIDDLE_DOT, C_DIM, C_BG, 0);
    }
    nl_text(r, COL_WHO, s == me ? "YOU" : kind == SEAT_BOT ? "BOT" : "", s == me ? C_WHITE : C_DIM, C_BG);
}

/* ---------------------------------------------------------------- the log */

static void log_row(unsigned char r, const unsigned char *s, unsigned char len)
{
    unsigned char fg, i = 0, col = COL_LOGTEXT;
    nl_fill(r, 0, NL_COLS, C_WHITE, C_BG);
    if (len >= 3 && s[0] == '[' && s[2] == ']') {   /* [$] [!] and the like: a coloured marker, then the text */
        fg = s[1] == '$' ? C_GREEN : s[1] == '!' ? C_MAGENTA : s[1] == '-' ? C_YELLOW : C_CYAN;
        for (i = 0; i < 3; ++i)
            nl_put(r, (unsigned char)(COL_LOGMARK + i), s[i], fg, C_BG, NL_BOLD);
        if (i < len && s[i] == ' ')
            ++i;
    }
    for (; i < len && col < NL_COLS; ++i, ++col)
        nl_put(r, col, s[i], C_WHITE, C_BG, 0);
}

void display_log(void)
{
    unsigned char r, c;
    for (r = 0; r < LOG_ROWS - 1; ++r)
        for (c = 0; c < NL_COLS; ++c)
            nl_draw[ROW_LOG + r][c] = nl_draw[ROW_LOG + r + 1][c];
    log_row(ROW_LOG + LOG_ROWS - 1, msg + 2, msg[1] > 123 ? 123 : msg[1]);
}

/* ---------------------------------------------------------------- the playfield */

static void strip_clear(void)
{
    nl_fill(ROW_INFO, 0, NL_COLS, C_WHITE, C_STRIP);
    nl_fill(ROW_ASK, 0, NL_COLS, C_WHITE, C_STRIP);
}

/* text on the strip from column `c`; returns the next column */
static unsigned char say(unsigned char r, unsigned char c, const char *s, unsigned char fg)
{
    for (; *s && c < NL_COLS; ++s)
        nl_put(r, c++, (unsigned char)*s, fg, C_STRIP, 0);
    return c;
}

/* the syndicates in `mask` as their coloured letter blocks, each followed by a space */
static unsigned char blocks(unsigned char r, unsigned char c, unsigned char mask)
{
    unsigned char i;
    for (i = 0; i < NSYN; ++i)
        if (mask >> i & 1) {
            nl_put(r, c, ' ', C_BLACK, (unsigned char)(C_SYN0 + i), 0);
            nl_put(r, c + 1, (unsigned char)syn_letter[i], C_BLACK, (unsigned char)(C_SYN0 + i), NL_BOLD);
            nl_put(r, c + 2, ' ', C_BLACK, (unsigned char)(C_SYN0 + i), 0);
            c = (unsigned char)(c + 4);
        }
    return c;
}

static void kit_line(void)
{
    unsigned char c, k, v, r, col;
    c = say(ROW_INFO, COL_PROMPT, T_EXPLOIT_P, C_CYAN);
    for (k = 0; k < RULE_KITSIZE; ++k) {
        v = kit[k];
        nl_put(ROW_INFO, c + 1, (unsigned char)('1' + k), C_YELLOW, C_STRIP, NL_BOLD);
        nl_put(ROW_INFO, c + 2, ':', C_DIM, C_STRIP, 0);
        if (v == NONE) {
            nl_put(ROW_INFO, c + 3, '-', C_DIM, C_STRIP, 0);
            nl_put(ROW_INFO, c + 4, '-', C_DIM, C_STRIP, 0);
        } else {
            node_rc(v & 0x7F, &r, &col);
            nl_put(ROW_INFO, c + 3, (unsigned char)('A' + col), v & 0x80 ? C_DIM : C_WHITE, C_STRIP, 0);
            nl_put(ROW_INFO, c + 4, (unsigned char)('0' + r), v & 0x80 ? C_DIM : C_WHITE, C_STRIP, 0);
        }
        c = (unsigned char)(c + 5);
    }
}

/* the question line: `s`, or the last complaint instead */
static unsigned char ask(const char *s)
{
    return say(ROW_ASK, COL_PROMPT, complaint ? complaint : s, C_WHITE);
}

static void typed(unsigned char r, unsigned char c)
{
    unsigned char i;
    for (i = 0; i < edit_len; ++i)
        nl_put(r, (unsigned char)(c + i), edit[i], C_WHITE, C_STRIP, NL_BOLD);
}

void display_prompt(void)
{
    unsigned char c, i, n;
    strip_clear();
    if (leaving) {                      /* Esc: the question waits; the prompt comes back after */
        say(ROW_ASK, COL_PROMPT, T_LOGOFF " NOW? Y/N", C_WHITE);
        return;
    }
    if (pr_kind == 'P' || (pr_kind == 0 && turn != NONE))
        kit_line();
    switch (pr_kind) {
    case 0:
        if (turn < SEATS && turn != me) {
            c = say(ROW_ASK, COL_PROMPT, "WAITING FOR ", C_DIM);
            for (n = 8; n && seat_name[turn][n - 1] == ' '; --n)
                ;
            for (i = 0; i < n; ++i)
                nl_put(ROW_ASK, c++, (unsigned char)seat_name[turn][i], C_WHITE, C_STRIP, NL_BOLD);
        }
        return;
    case 'S':
        ask("START WITH HOW MANY SEATS? 2-6");
        break;
    case 'P':
        ask("RUN WHICH " T_EXPLOIT "? PRESS 1-6");
        break;
    case 'F':                           /* pr[1] = available */
        c = say(ROW_INFO, COL_PROMPT, T_INFILTRATION "! " T_BROKER " TO ", C_WHITE);
        blocks(ROW_INFO, c, pr[1]);
        ask("PRESS A SYNDICATE LETTER");
        break;
    case 'K':                           /* pr[1] = ctx, pr[2] = options */
        c = say(ROW_INFO, COL_PROMPT, pr[1] ? "TIE! ABSORB WHICH NEXT? " : "TIE! WHICH SURVIVES? ", C_WHITE);
        blocks(ROW_INFO, c, pr[2]);
        ask("PRESS A SYNDICATE LETTER");
        break;
    case 'D':                           /* pr: absorbed, absorber, held, supply */
        c = say(ROW_INFO, COL_PROMPT, T_HIVEJACK ": ", C_WHITE);
        nl_put(ROW_INFO, c++, (unsigned char)syn_letter[pr[2]], (unsigned char)(C_SYN0 + pr[2]), C_STRIP, NL_BOLD);
        c = say(ROW_INFO, c, " TAKES ", C_WHITE);
        nl_put(ROW_INFO, c++, (unsigned char)syn_letter[pr[1]], (unsigned char)(C_SYN0 + pr[1]), C_STRIP, NL_BOLD);
        c = say(ROW_INFO, c, ". YOU HOLD ", C_WHITE);
        number(ROW_INFO, c, pr[3], C_WHITE, C_STRIP);
        c = (unsigned char)(c + digits_of(pr[3]));
        nl_put(ROW_INFO, c + 1, (unsigned char)syn_letter[pr[1]], (unsigned char)(C_SYN0 + pr[1]), C_STRIP, NL_BOLD);
        c = ask(step ? T_SWAP " 2 FOR 1, HOW MANY? " : T_CASHOUT " HOW MANY? ");
        if (!complaint)
            typed(ROW_ASK, c);
        break;
    case 'B':                           /* pr: timeout, cash[2], flags, price[7], supply[7] */
        if (!(step && !edit_len)) {     /* after the letters, with nothing bought: just the question */
            c = say(ROW_INFO, COL_PROMPT, "BUY UP TO " "3 " T_COIN ": ", C_WHITE);
            typed(ROW_INFO, c);
        }
        ask(step ? T_NULLROUTE " AND END THE GAME? Y/N" : "LETTERS, BACKSPACE, ENTER TO BUY");
        break;
    }
}

/* One second short: the clock ticks a little slow, so shown short, it never promises more time than the server gives. */
void display_clock(unsigned char secs)
{
    if (!pr_kind)
        return;
    nl_fill(ROW_ASK, 76, 3, C_WHITE, C_STRIP);
    if (secs)
        number_ending_strip(ROW_ASK, 78, (unsigned char)(secs - 1));
}

void display_message(const char *s)
{
    nl_fill(ROW_ASK, 0, NL_COLS, C_WHITE, C_STRIP);
    say(ROW_ASK, COL_PROMPT, s, C_WHITE);
}

void display_game_over(void)
{
    unsigned char c, i, n;
    nl_fill(ROW_INFO, 0, NL_COLS, C_WHITE, C_STRIP);
    c = say(ROW_INFO, COL_PROMPT, winners & (winners - 1) ? "TOP " T_PLAYER_P ":" : "GAME OVER. TOP " T_PLAYER ":", C_WHITE);
    for (i = 0; i < SEATS; ++i)
        if (winners >> i & 1) {
            ++c;
            for (n = 0; n < 8 && seat_name[i][n] != ' ' && c < NL_COLS; ++n)
                nl_put(ROW_INFO, c++, (unsigned char)seat_name[i][n], C_YELLOW, C_STRIP, NL_BOLD);
        }
}

#if LOG_HISTORY
void display_log_view(void)
{
    unsigned char r;
    const unsigned char *p;
    for (r = 0; r < LOG_ROWS; ++r) {
        p = log_line((unsigned char)(log_back + LOG_ROWS - 1 - r));
        if (p)
            log_row((unsigned char)(ROW_LOG + r), p + 1, p[0]);
        else
            nl_fill((unsigned char)(ROW_LOG + r), 0, NL_COLS, C_WHITE, C_BG);
    }
}
#endif

void display_playfield(void)
{
    unsigned char k, r;
    nl_clear(C_WHITE, C_BG);
    title_bar();
    for (k = 0; k < RULE_COLS; ++k)
        nl_put(ROW_HEAD, (unsigned char)(COL_GRID + CELL_W * k + 1), (unsigned char)('A' + k), C_DIM, C_BG, 0);
    for (r = 0; r < RULE_ROWS; ++r)
        nl_put((unsigned char)(ROW_GRID + r), COL_ROWNUM, (unsigned char)('0' + r), C_DIM, C_BG, 0);
    nl_text(ROW_HEAD, COL_HIVE, T_HIVE_P, C_CYAN, C_BG);
    for (k = 0; k < NODES; ++k)
        display_node(k);
    display_hives();
    nl_text(ROW_SEATHEAD, COL_SEAT, T_PLAYER_P, C_CYAN, C_BG);
    nl_text(ROW_SEATHEAD, (unsigned char)(COL_CASH + CASH_W - sizeof T_ACCOUNT + 1), T_ACCOUNT, C_DIM, C_BG);
    for (k = 0; k < NSYN; ++k) {
        nl_put(ROW_SEATHEAD, (unsigned char)(COL_HOLD0 + HOLD_STEP * k), ' ', C_BLACK, (unsigned char)(C_SYN0 + k), 0);
        nl_put(ROW_SEATHEAD, (unsigned char)(COL_HOLD0 + HOLD_STEP * k + 1), (unsigned char)syn_letter[k], C_BLACK, (unsigned char)(C_SYN0 + k), NL_BOLD);
        nl_put(ROW_SEATHEAD, (unsigned char)(COL_HOLD0 + HOLD_STEP * k + 2), ' ', C_BLACK, (unsigned char)(C_SYN0 + k), 0);
    }
    for (k = 0; k < SEATS; ++k)
        display_seat(k);
    display_prompt();
}

/* ---------------------------------------------------------------- the title */

static void blank_screen(void)
{
    nl_clear(C_WHITE, C_BG);
}

static void label(unsigned char r, const char *s)
{
    nl_text(r, COL_LABEL, s, C_CYAN, C_BG);
}

void display_title_update(void)
{
    unsigned char n = 0, i, fg = C_WHITE;
    nl_fill(ROW_REGION, COL_VALUE, (unsigned char)(NL_COLS - COL_VALUE), C_WHITE, C_BG);
    for (i = 0; i < region_len; ++i)
        nl_put(ROW_REGION, (unsigned char)(COL_VALUE + i), region[i], C_WHITE, C_BG, NL_BOLD);
    nl_fill(ROW_REGION + 1, COL_VALUE, (unsigned char)(NL_COLS - COL_VALUE), C_WHITE, C_BG);
    if (status_len) {
        n = (unsigned char)(status_len - 1);
        for (i = 0; i < n; ++i)
            nl_put(ROW_REGION + 1, (unsigned char)(COL_VALUE + i), status_text[i], C_GREEN, C_BG, 0);
    } else if (status_code == NO_CARRIER)
        nl_text(ROW_REGION + 1, COL_VALUE, "NO CARRIER", C_RED, C_BG);
    nl_fill(ROW_REGION + 2, COL_VALUE, (unsigned char)(NL_COLS - COL_VALUE), C_WHITE, C_BG);
    for (i = 0; i < edit_len; ++i)
        nl_put(ROW_REGION + 2, (unsigned char)(COL_VALUE + i), edit[i], fg, C_BG, NL_BOLD);
    nl_put(ROW_REGION + 2, (unsigned char)(COL_VALUE + edit_len), ' ', C_BLACK, C_WHITE, 0);   /* the cursor */
    nl_fill(ROW_KEYS, COL_HINTS, 3, C_WHITE, C_BG);
    nl_text(ROW_KEYS, COL_HINTS, hints ? "ON" : "OFF", hints ? C_GREEN : C_DIM, C_BG);
}

void display_title_screen(void)
{
    const unsigned short *p;
    unsigned char n, c;
    unsigned short v;
    blank_screen();
    for (p = title_art; *p != 0xFFFF; p += 3 + p[2]) {      /* each art cell, then a joining line to its right */
        c = (unsigned char)(2 * p[1] + TITLE_SHIFT);
        for (n = 0; n < p[2]; ++n) {
            v = p[3 + n];
            nl_put((unsigned char)p[0], c++, v, v == 0x2580 ? C_MAGENTA : C_CYAN, C_BG, NL_BOLD);
            v = v == 0x250C || v == 0x251C || v == 0x252C || v == 0x253C || v == 0x2500 || v == 0x2514 || v == 0x2534
                ? 0x2500 : v == 0x2580 || (n + 1 < p[2] && p[4 + n] == 0x2580) ? 0x2580 : ' ';
            nl_put((unsigned char)p[0], c++, v, v == 0x2580 ? C_MAGENTA : C_CYAN, C_BG, NL_BOLD);
        }
    }
    label(ROW_REGION, "REGION");
    label(ROW_REGION + 1, "STATUS");
    label(ROW_REGION + 2, "HANDLE");
    nl_text(ROW_KEYS, COL_LABEL, "TAB", C_CYAN, C_BG);
    nl_text(ROW_KEYS + 1, COL_LABEL, "^S", C_CYAN, C_BG);
    nl_text(ROW_KEYS + 2, COL_LABEL, "?", C_CYAN, C_BG);
    nl_text(ROW_KEYS, COL_KEYS, "MOVE HINTS", C_DIM, C_BG);
    nl_text(ROW_KEYS + 1, COL_KEYS, "TOGGLE SOUND", C_DIM, C_BG);
    nl_text(ROW_KEYS + 2, COL_KEYS, "INSTRUCTIONS, ANY TIME", C_DIM, C_BG);
    nl_fill(ROW_ASK, 0, NL_COLS, C_WHITE, refused ? C_RED : C_STRIP);
    c = COL_PROMPT;
    for (n = 0; ; ++n) {
        const char *t = refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN"
            : refused == REFUSED_HANDLE ? "THAT HANDLE IS TAKEN" : "TYPE A HANDLE, THEN ENTER. ESC QUITS";
        if (!t[n])
            break;
        nl_put(ROW_ASK, c++, (unsigned char)t[n], C_WHITE, refused ? C_RED : C_STRIP, 0);
    }
    display_title_update();
}

/* ---------------------------------------------------------------- the instruction pages */

static nl_cell shadow[NL_ROWS][NL_COLS];
static unsigned char page;

static void help_draw(void)
{
    unsigned char r, c, l, i;
    const char *t;
    nl_draw = nl_cells;                 /* the panel goes on the screen; the game's drawing waits in the shadow */
    nl_fill(HELP_ROW, HELP_COL, HELP_W, C_WHITE, C_BG);
    for (r = 0; r < HELP_H; ++r)
        nl_fill((unsigned char)(HELP_ROW + r), HELP_COL, HELP_W, C_WHITE, C_BG);
    for (c = 1; c < HELP_W - 1; ++c) {
        nl_put(HELP_ROW, (unsigned char)(HELP_COL + c), 0x2500, C_CYAN, C_BG, 0);
        nl_put((unsigned char)(HELP_ROW + HELP_H - 1), (unsigned char)(HELP_COL + c), 0x2500, C_CYAN, C_BG, 0);
    }
    for (r = 1; r < HELP_H - 1; ++r) {
        nl_put((unsigned char)(HELP_ROW + r), HELP_COL, 0x2502, C_CYAN, C_BG, 0);
        nl_put((unsigned char)(HELP_ROW + r), (unsigned char)(HELP_COL + HELP_W - 1), 0x2502, C_CYAN, C_BG, 0);
    }
    nl_put(HELP_ROW, HELP_COL, 0x250C, C_CYAN, C_BG, 0);
    nl_put(HELP_ROW, (unsigned char)(HELP_COL + HELP_W - 1), 0x2510, C_CYAN, C_BG, 0);
    nl_put((unsigned char)(HELP_ROW + HELP_H - 1), HELP_COL, 0x2514, C_CYAN, C_BG, 0);
    nl_put((unsigned char)(HELP_ROW + HELP_H - 1), (unsigned char)(HELP_COL + HELP_W - 1), 0x2518, C_CYAN, C_BG, 0);
    for (l = 0; l < HELP_LINES && (t = help_text[page][l]) != 0; ++l) {
        r = (unsigned char)(HELP_ROW + (l ? 2 + l : 1));
        for (i = 0; t[i]; ++i)
            nl_put(r, (unsigned char)(HELP_COL + 2 + i), t[i] == '\\' ? YEN : (unsigned char)t[i],
                   l ? C_WHITE : C_YELLOW, C_BG, l ? 0 : NL_BOLD);
    }
    nl_put((unsigned char)(HELP_ROW + 1), (unsigned char)(HELP_COL + HELP_W - 6), (unsigned char)('1' + page), C_DIM, C_BG, 0);
    nl_put((unsigned char)(HELP_ROW + 1), (unsigned char)(HELP_COL + HELP_W - 5), '/', C_DIM, C_BG, 0);
    nl_put((unsigned char)(HELP_ROW + 1), (unsigned char)(HELP_COL + HELP_W - 4), (unsigned char)('0' + HELP_PAGES), C_DIM, C_BG, 0);
    r = (unsigned char)(HELP_ROW + HELP_H - 1);
    nl_text(r, (unsigned char)(HELP_COL + 3), page ? "< PREV" : "      ", C_CYAN, C_BG);
    nl_text(r, (unsigned char)(HELP_COL + HELP_W / 2 - 4), "ESC EXIT", C_CYAN, C_BG);
    nl_text(r, (unsigned char)(HELP_COL + HELP_W - 9), page < HELP_PAGES - 1 ? "NEXT >" : "      ", C_CYAN, C_BG);
    nl_draw = shadow;
}

void help_open(void)
{
    memcpy(shadow, nl_cells, sizeof shadow);
    page = 0;
    help_draw();
}

unsigned char help_key(unsigned char k)
{
    if (k == K_ESC || k == K_RETURN || k == K_HELP) {
        nl_draw = nl_cells;
        memcpy(nl_cells, shadow, sizeof shadow);
        return 0;
    }
    if ((k == '>' || k == '.') && page < HELP_PAGES - 1)
        ++page;
    else if ((k == '<' || k == ',') && page)
        --page;
    else
        return 1;
    help_draw();
    return 1;
}
