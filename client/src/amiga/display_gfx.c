/* The Amiga's display: the circuit-board playfield and a title, drawn with gfx.c on the 5-plane screen. Nothing is
 * kept but the log's two lines and a few things to tell what changed, so every call paints only its own piece
 * (tools/amiga_mock.py is the picture; layout.c has its numbers). The names and the question wording are
 * src/msdos/display_text.c's, from the theme. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"
#include "amiga.h"
#include "gfx.h"
#include "ui.h"
#include "help.h"
#include "title_art.h"

#define LY      amiga_layout
#define YEN     0xA5                    /* Topaz is ISO Latin-1: the server's "\" is a yen here */
#define FLASH_SECS 4                    /* the 8-bit tick counter holds 4 s at 60 a second, not 5 */
#define STRIP_CH 27                     /* what fits left of the clock */
#define WIDE_CH  38

static const char *const syn_short[NSYN] = SYN_SHORTS;
static unsigned char field;             /* the playfield is up */
static unsigned char ice_mask;          /* the syndicates drawn as Black ICE */
static unsigned char helping;
static unsigned char over_t;            /* when the game ended, for a client that is not asked to start the next */
static unsigned char title_up;          /* the title's loop is running: set on each of its passes, cleared when Return leaves it */
static char logl[3][WIDE_CH + 2];       /* the log's three lines: a move is a spike, a spread and up to three buys */
static unsigned char logn[3];
static unsigned char kit_shown[RULE_KITSIZE], kit_valid;
static unsigned char kit_pick;          /* the kit panel shows the pick's message, not the kit */
static unsigned char marked;            /* the syndicate rows lit for a pick */
static unsigned char blink = NONE, blink_t, done = NONE, lit;
static unsigned char others_top;
/* The display_* calls only note what changed; display_idle paints it once per pass of the game loop, after every queued
 * frame has been applied (a move is a dozen frames: coins, cash, seats, hive sizes). Painting costs a good many
 * graphics.library calls on a 5-plane screen, so each piece is painted once, not once per frame. */
static unsigned char dirty_node[(NODES + 7) / 8], dirty_seat, dirty_hives, dirty_log, dirty_prompt;        /* NTSC: the first of the list's seats shown */

/* ---------------------------------------------------------------- small helpers */

static void rect_set(struct rect *r, short x, short y, short w, short h)
{
    r->x = x; r->y = y; r->w = w; r->h = h;
}

/* Draw s (n chars, the server's "\" turned to a yen), padded with spaces to `width` chars. */
static void say(short x, short y, const char *s, unsigned char n, unsigned char width, unsigned char pen, unsigned char bg)
{
    char b[WIDE_CH + 2];
    unsigned char i;
    if (n > width)
        n = width;
    for (i = 0; i < n; ++i)
        b[i] = s[i] == '\\' ? YEN : s[i];
    for (; i < width; ++i)
        b[i] = ' ';
    gfx_text(x, y, b, width, pen, bg);
}

/* value (units of 100 if money) as text in numtxt; returns its length */
static char numtxt[12];
static unsigned char number(unsigned long v, unsigned char money)
{
    char t[12];
    unsigned char n = 0, k;
    if (money && v)
        v *= 100;
    do {
        t[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    k = 0;
    if (money)
        numtxt[k++] = YEN;
    while (n)
        numtxt[k++] = t[--n];
    return k;
}

static void trimmed_name(unsigned char seat, char *out, unsigned char *len)
{
    unsigned char n = 8;
    memcpy(out, seat_name[seat], 8);
    while (n && out[n - 1] == ' ')
        --n;
    *len = n;
}

static unsigned char text_h(const struct rect *r)
{
    return (r->h - 7) / 2;
}

/* ---------------------------------------------------------------- the grid */

/* The key ('1'-'6') of the exploit in my kit that can run on `node` now, or 0. */
static unsigned char target(unsigned char node)
{
    unsigned char n;
    if (!hints)
        return 0;
    for (n = 0; n < RULE_KITSIZE; ++n)
        if (kit[n] == node)             /* bit 7 set = can't run: never equal */
            return '1' + n;
    return 0;
}

static void paint_node(unsigned char node)
{
    struct rect c, in, t;
    unsigned char v = board[node], row = node / RULE_COLS, col = node % RULE_COLS, k;
    char ch;

    layout_node_rect(&LY, node, &c);
    gfx_rect(&c, PEN_BG1);              /* the traces run through the pads */
    rect_set(&t, c.x, c.y + c.h / 2, c.w, 1);
    gfx_rect(&t, PEN_BG2);
    rect_set(&t, c.x + c.w / 2, c.y, 1, c.h);
    gfx_rect(&t, PEN_BG2);
    rect_set(&in, c.x + 2, c.y + 2, c.w - 4, c.h - 4);
    if (v >= 2 && v < 2 + NSYN) {       /* a hive: one slab in its syndicate's colour, Black ICE in steel */
        unsigned char s = v - 2, big = hive_size[s] >= RULE_SAFESIZE, dk, bright;
        dk = big ? PEN_STEEL2 : PEN_SYN_DIM + s;
        bright = big ? PEN_STEEL : PEN_SYN + s;
        gfx_rect(&c, dk);
        rect_set(&in, c.x + 1, c.y + 1, col + 1 < RULE_COLS && board[node + 1] == v ? c.w - 1 : c.w - 2,
                 row + 1 < RULE_ROWS && board[node + RULE_COLS] == v ? c.h - 1 : c.h - 2);
        gfx_rect(&in, bright);
        if (big)
            gfx_hatch(&in, PEN_STEEL2, PEN_STEEL);
        ch = syn_letter[s];
        gfx_text(c.x + 4, c.y + text_h(&c), &ch, 1, PEN_BG0, bright);
    } else if (v == CELL_SPIKE) {
        gfx_rect(&in, PEN_BG2);
        gfx_frame(&in, PEN_EDGEHI);
        rect_set(&t, c.x + c.w / 2 - 2, c.y + c.h / 2 - 2, 4, 5);
        gfx_rect(&t, PEN_WHITE);
    } else if ((k = target(node)) != 0) {
        gfx_rect(&in, PEN_BG2);
        gfx_frame(&in, PEN_CYAN);
        ch = k;
        gfx_text(c.x + 4, c.y + text_h(&c), &ch, 1, PEN_CYAN, PEN_BG2);
    } else {
        gfx_rect(&in, PEN_BG1);
        gfx_frame(&in, PEN_BG2);
        rect_set(&t, c.x + c.w / 2 - 1, c.y + c.h / 2 - 1, 2, 2);
        gfx_rect(&t, PEN_EDGE);
    }
    if (node == last_node && (blink != node || lit)) {   /* the last move: an amber ring, blinking at first */
        gfx_frame(&c, PEN_AMBER);
        rect_set(&in, c.x + 1, c.y + 1, c.w - 2, c.h - 2);
        gfx_frame(&in, PEN_WHITE);
    }
}

void display_node(unsigned char node)
{
    if (node >= NODES)
        return;
    if (node == last_node) {
        if (node != done && blink != node) {
            blink = node;
            blink_t = ticks();
            lit = 1;
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    dirty_node[node >> 3] |= 1 << (node & 7);
}

static void node_now(unsigned char node)
{
    paint_node(node);
    if (board[node] >= 2 && board[node] < 2 + NSYN) {   /* the slab joins up with the cells left of it and above */
        if (node % RULE_COLS && board[node - 1] == board[node])
            paint_node(node - 1);
        if (node >= RULE_COLS && board[node - RULE_COLS] == board[node])
            paint_node(node - RULE_COLS);
    }
}

static void prompt_now(void);
static void hives_now(void);
static void seats_now(void);
static void paint_log(void);

static void flush(void)
{
    unsigned char i;
    if (dirty_prompt) {
        dirty_prompt = 0;
        prompt_now();                   /* first: a dialog opening or closing decides what else is painted */
    }
    if (dialog_up())
        return;                         /* the rest waits; closing the dialog repaints everything */
    for (i = 0; i < NODES; ++i)
        if (dirty_node[i >> 3] >> (i & 7) & 1) {
            dirty_node[i >> 3] &= ~(1 << (i & 7));
            node_now(i);
        }
    if (dirty_hives) {
        dirty_hives = 0;
        hives_now();
    }
    if (dirty_seat) {
        seats_now();
        dirty_seat = 0;
    }
    if (dirty_log) {
        dirty_log = 0;
        paint_log();
    }
}

#ifdef DRAWLOG                          /* a debug build: how much of the time the drawing takes, in the guest's log */
#include <stdio.h>
static unsigned long t_draw, t_all;
static unsigned char t_last, t_max;
static void flush_timed(void)
{
    unsigned char now = ticks(), t0;
    t_all += (unsigned char)(now - t_last);
    t_last = now;
    t0 = now;
    flush();
    t_draw += (unsigned char)(ticks() - t0);
    if ((unsigned char)(ticks() - t0) > t_max)
        t_max = ticks() - t0;
    if (t_all >= 20UL * ticks_per_sec()) {
        printf("draw %lu of %lu ticks, longest burst %u\n", t_draw, t_all, t_max);
        fflush(stdout);
        t_draw = t_all = 0;
        t_max = 0;
    }
}
#define flush flush_timed
#endif

void display_idle(void)
{
    unsigned char e, phase;
    if (dialog_holding() && (pr_kind ? pr_kind != 'S' : (unsigned char)(ticks() - over_t) >= 2 * ticks_per_sec()))
        dialog_hold(0);                 /* not the host (no seat prompt came), or play went on: nothing to wait for */
    if (!field || helping)
        return;
    flush();
    if (blink == NONE || dialog_up())
        return;
    if (blink != last_node) {           /* a newer move took over, or a new game */
        e = blink;
        blink = NONE;
        paint_node(e);
        return;
    }
    e = ticks() - blink_t;
    if (e >= FLASH_SECS * ticks_per_sec()) {
        done = blink;
        blink = NONE;
        paint_node(done);
        return;
    }
    phase = !((e / (ticks_per_sec() / 2)) & 1);     /* lit and unlit by turns, twice a second */
    if (phase != lit) {
        lit = phase;
        paint_node(blink);
    }
}

static void paint_grid(void)
{
    unsigned char c, r;
    char ch;
    gfx_rect(&LY.grid, PEN_BG1);
    gfx_frame(&LY.grid, PEN_EDGE);
    for (c = 0; c < RULE_COLS; ++c) {
        ch = 'A' + c;
        gfx_text(LY.grid_x + c * LY.cell_w + 4, LY.grid.y + 1, &ch, 1, PEN_DIM, PEN_BG1);
    }
    for (r = 0; r < RULE_ROWS; ++r) {
        ch = '0' + r;
        gfx_text(LY.grid.x + 2, LY.grid_y + r * LY.cell_h + (LY.cell_h - 7) / 2, &ch, 1, PEN_DIM, PEN_BG1);
    }
    for (c = 0; c < NODES; ++c)
        paint_node(c);
}

/* ---------------------------------------------------------------- the panels */

/* A hive that has reached Black ICE size (or lost it) is drawn in steel (or its colour) all over. */
static void repaint_ice(void)
{
    unsigned char j, i, now = 0;
    for (j = 0; j < NSYN; ++j)
        if (hive_size[j] >= RULE_SAFESIZE)
            now |= 1 << j;
    for (j = 0; j < NSYN; ++j)
        if ((now ^ ice_mask) >> j & 1)
            for (i = 0; i < NODES; ++i)
                if (board[i] == 2 + j)
                    paint_node(i);
    ice_mask = now;
}

void display_hives(void)
{
    dirty_hives = 1;
}

static void hives_now(void)
{
    const struct rect *p = &LY.syn_panel;
    struct rect row;
    unsigned char j, n, l;
    unsigned char want = pr_kind == 'F' ? pr[1] : pr_kind == 'K' ? pr[2] : 0;
    char ch;

    marked = want;
    gfx_bevel(p, PEN_BG1, want ? PEN_AMBER : PEN_EDGE, PEN_BG0);     /* a pick: the box to look at */
    gfx_text(p->x + 4, p->y + 2, T_HIVE_P, sizeof T_HIVE_P - 1, PEN_CYAN, PEN_BG1);
    for (j = 0; j < NSYN; ++j) {
        unsigned char lit = want >> j & 1, bg = lit ? PEN_EDGEHI : PEN_BG1;
        unsigned char pen = !want || lit ? (lit ? PEN_BG0 : PEN_TEXT) : PEN_DIM;      /* a pick: the choices stand out, the rest recede */
        row = LY.syn_row[j];
        if (lit)
            gfx_rect(&row, bg);         /* the free ones: inverse, a band to look at */
        gfx_text(row.x, row.y, &syn_letter[j], 1, PEN_BG0, PEN_SYN + j);   /* the colour, with the letter that picks it */
        n = hive_size[j];
        if (n >= RULE_SAFESIZE && pen == PEN_TEXT)
            pen = PEN_STEEL;            /* Black ICE: grey, as on the grid */
        say(row.x + 10, row.y, syn_short[j], strlen(syn_short[j]), 8, pen, bg);
        if (n) {
            l = number(n, 0);
            gfx_text_right(row.x + row.w, row.y, numtxt, l, n >= RULE_SAFESIZE ? PEN_STEEL : PEN_WHITE, bg);
        } else {
            ch = '-';
            gfx_text_right(row.x + row.w, row.y, &ch, 1, lit ? PEN_BG0 : PEN_DIM, bg);
        }
    }
    repaint_ice();
}

static void draw_kit(unsigned char force)
{
    const struct rect *p = &LY.kit_panel;
    unsigned char j, v, pick = pr_kind == 'F' || pr_kind == 'K', same = kit_valid && !force && pick == kit_pick;
    char b[2];
    for (j = 0; same && !pick && j < RULE_KITSIZE; ++j)
        same = kit_shown[j] == kit[j];
    if (same)
        return;
    kit_pick = pick;
    if (pick) {                         /* the pick: this panel says what to do, and the syndicates' box is the one to look at */
        gfx_bevel(p, PEN_BG1, PEN_AMBER, PEN_BG0);
        gfx_text(p->x + 8, p->y + 5, pr_kind == 'F' ? "PICK A FREE" : "TIE! PICK", pr_kind == 'F' ? 11 : 9, PEN_AMBER, PEN_BG1);
        gfx_text(p->x + 8, p->y + 15, pr_kind == 'F' ? "SYNDICATE" : "ONE OF THE", pr_kind == 'F' ? 9 : 10, PEN_AMBER, PEN_BG1);
        gfx_text(p->x + 8, p->y + 25, pr_kind == 'F' ? "FROM THE" : "BRIGHT ROWS", pr_kind == 'F' ? 8 : 11, PEN_WHITE, PEN_BG1);
        gfx_text(p->x + 8, p->y + 35, pr_kind == 'F' ? "BRIGHT ROWS" : "", pr_kind == 'F' ? 11 : 0, PEN_WHITE, PEN_BG1);
        kit_valid = 1;
        return;
    }
    gfx_bevel(p, PEN_BG1, PEN_EDGE, PEN_BG0);
    gfx_text(p->x + 4, p->y + 2, T_KIT, sizeof T_KIT - 1, PEN_CYAN, PEN_BG1);
    for (j = 0; j < RULE_KITSIZE; ++j) {
        const struct rect *c = &LY.kit_chip[j];
        kit_shown[j] = v = kit[j];
        gfx_bevel(c, PEN_BG2, PEN_EDGEHI, PEN_BG0);
        b[0] = '1' + j;
        gfx_text(c->x + 3, c->y + text_h(c), b, 1, PEN_CYAN, PEN_BG2);
        if (v == NONE) {
            gfx_text(c->x + 14, c->y + text_h(c), "--", 2, PEN_DIM, PEN_BG2);
        } else {
            b[0] = 'A' + (v & 0x7F) % RULE_COLS;
            b[1] = '0' + (v & 0x7F) / RULE_COLS;
            gfx_text(c->x + 14, c->y + text_h(c), b, 2, v & 0x80 ? PEN_DIM : PEN_WHITE, PEN_BG2);
        }
    }
    kit_valid = 1;
}

/* ---------------------------------------------------------------- the seats */

static void card(const struct rect *r, unsigned char s)
{
    unsigned char kind = seat_kind[s], j, n, l;
    char b[9];
    gfx_bevel(r, PEN_BG1, s == turn ? PEN_AMBER : PEN_EDGE, PEN_BG0);
    if (kind == SEAT_EMPTY) {
        gfx_text(r->x + 4, r->y + 3, "-", 1, PEN_DIM, PEN_BG1);
        return;
    }
    b[0] = s == me ? '>' : kind == SEAT_BOT ? '*' : kind == SEAT_AWAY ? '?' : ' ';
    memcpy(b + 1, seat_name[s], 8);
    gfx_text(r->x + 4, r->y + 2, b, 9, s == turn ? PEN_WHITE : PEN_TEXT, PEN_BG1);
    l = number(cash[s], 1);
    say(r->x + 4, r->y + 11, numtxt, l, 11, PEN_AMBER, PEN_BG1);
    for (j = 0; j < NSYN; ++j) {
        n = coins[s][j];
        if (n) {
            l = number(n, 0);
            gfx_text(r->x + 4 + j * 13 - (n >= 10 ? 4 : 0), r->y + 20, numtxt, l, PEN_SYN + j, PEN_BG1);
        } else
            gfx_text(r->x + 4 + j * 13, r->y + 20, ".", 1, PEN_DIM, PEN_BG1);
    }
}

/* ---- the table view (F4, or a click on the seats): the other text clients' table, a row for each netrunner with
 * its account and what it holds in each syndicate, the digits in the syndicates' colours. PAL has the room for a header
 * and six rows; NTSC for three: me, the seat to move, and the others one at a time under the arrows. */

#define TBL_CASH 136                    /* the account's right edge, from the table's left */
#define TBL_COL 23                      /* a syndicate's column */

#ifdef TABLEVIEW
static unsigned char table_view = 1;       /* a test build starts in the table, to look at it on the rig */
#else
static unsigned char table_view;
#endif

static void table_row(short x, short y, unsigned char s, unsigned char w)
{
    unsigned char kind = seat_kind[s], j, n, l, bg = s == turn ? PEN_BG2 : PEN_BG1;
    char b[10];
    struct rect r;
    rect_set(&r, x, y, w, 8);
    gfx_rect(&r, bg);
    b[0] = s == me ? '>' : ' ';
    memcpy(b + 1, seat_name[s], 8);
    b[9] = kind == SEAT_BOT ? '*' : kind == SEAT_AWAY ? '?' : ' ';
    gfx_text(x + 2, y, b, 10, s == turn ? PEN_AMBER : PEN_TEXT, bg);
    l = number(cash[s], 1);
    gfx_text_right(x + TBL_CASH, y, numtxt, l, PEN_AMBER, bg);
    for (j = 0; j < NSYN; ++j) {
        n = coins[s][j];
        if (n) {
            l = number(n, 0);
            gfx_text_right(x + TBL_CASH + j * TBL_COL + 20, y, numtxt, l, PEN_SYN + j, bg);
        } else
            gfx_text_right(x + TBL_CASH + j * TBL_COL + 20, y, ".", 1, PEN_DIM, bg);
    }
}

static unsigned char table_rows;        /* the seats the PAL table last showed: the box is wiped only when they change */

static void paint_table_pal(const struct rect *area, unsigned char full)
{
    unsigned char s, j, row = 0, n = 0;
    short x = area->x + 4, y = area->y + (LY.ntsc ? 2 : 3);
    struct rect r;
    char ch;

    for (s = 0; s < SEATS; ++s)
        n += seat_kind[s] != SEAT_EMPTY;
    if (full || n != table_rows) {
        gfx_bevel(area, PEN_BG1, PEN_EDGE, PEN_BG0);
        table_rows = n;
    }
    gfx_text(x + 2, y, T_PLAYERTITLE_P, sizeof T_PLAYERTITLE_P - 1, PEN_CYAN, PEN_BG1);
    gfx_text_right(x + TBL_CASH, y, T_ACCOUNT, sizeof T_ACCOUNT - 1, PEN_DIM, PEN_BG1);
    for (j = 0; j < NSYN; ++j) {
        rect_set(&r, x + TBL_CASH + 4 + j * TBL_COL, y, 16, 8);
        gfx_rect(&r, PEN_SYN + j);
        ch = syn_letter[j];             /* the letter the hive panel and the grid show */
        gfx_text(r.x + 4, y, &ch, 1, PEN_BG0, PEN_SYN + j);
    }
    for (s = 0; s < SEATS; ++s)
        if (seat_kind[s] != SEAT_EMPTY)
            table_row(x, y + 8 + row++ * 8, s, area->w - 8);
}

static void paint_table_ntsc(void);
static unsigned char others_count(void);

/* the seats, whichever way they are shown (PAL: all of them; NTSC: the area) */
static void paint_seats_pal(void)
{
    unsigned char i;
    if (table_view)
        paint_table_pal(&LY.seats_area, 1);
    else
        for (i = 0; i < SEATS; ++i)
            card(&LY.seat[i], i);
}

/* NTSC has room for two cards: me, and the seat to move (or the next one, when it is me); the rest are a list. */
static unsigned char second_seat(void)
{
    unsigned char s, k;
    if (turn < SEATS && turn != me)
        return turn;
    for (k = 1; k <= SEATS; ++k) {
        s = (me < SEATS ? me + k : k - 1) % SEATS;
        if (s != me && seat_kind[s] != SEAT_EMPTY)
            return s;
    }
    return NONE;
}

static void paint_seat_area_ntsc(void)
{
    unsigned char two = second_seat(), s, shown = 0, total = 0, k, l;
    char b[9];
    struct rect up = LY.scroll_up, down = LY.scroll_down;

    if (table_view) {
        paint_table_ntsc();
        return;
    }
    if (me < SEATS)
        card(&LY.seat[0], me);
    else
        gfx_bevel(&LY.seat[0], PEN_BG1, PEN_EDGE, PEN_BG0);
    if (two < SEATS)
        card(&LY.seat[1], two);
    else
        gfx_bevel(&LY.seat[1], PEN_BG1, PEN_EDGE, PEN_BG0);
    for (s = 0; s < SEATS; ++s)
        if (s != me && s != two && seat_kind[s] != SEAT_EMPTY)
            ++total;
    if (others_top + 3 > total)
        others_top = total > 3 ? total - 3 : 0;
    gfx_bevel(&LY.others, PEN_BG1, PEN_EDGE, PEN_BG0);
    for (s = 0, k = 0; s < SEATS && shown < 3; ++s) {
        if (s == me || s == two || seat_kind[s] == SEAT_EMPTY)
            continue;
        if (k++ < others_top)
            continue;
        b[0] = seat_kind[s] == SEAT_BOT ? '*' : seat_kind[s] == SEAT_AWAY ? '?' : ' ';
        memcpy(b + 1, seat_name[s], 5);
        l = number(cash[s], 1);
        gfx_text(LY.others.x + 4, LY.others.y + 2 + shown * 8, b, total > 3 ? 4 : 5, PEN_TEXT, PEN_BG1);
        gfx_text_right(LY.others.x + LY.others.w - (total > 3 ? 14 : 2), LY.others.y + 2 + shown * 8, numtxt, l, PEN_AMBER, PEN_BG1);
        ++shown;
    }
    if (total > 3) {
        gfx_rect(&up, PEN_BG2);
        gfx_text(up.x + 1, up.y + 2, "^", 1, others_top ? PEN_WHITE : PEN_DIM, PEN_BG2);
        gfx_rect(&down, PEN_BG2);
        gfx_text(down.x + 1, down.y + 2, "v", 1, others_top + 3 < total ? PEN_WHITE : PEN_DIM, PEN_BG2);
    }
}

static unsigned char others_count(void)
{
    unsigned char two = second_seat(), s, n = 0;
    for (s = 0; s < SEATS; ++s)
        if (s != me && s != two && seat_kind[s] != SEAT_EMPTY)
            ++n;
    return n;
}

static void paint_table_ntsc(void)
{
    unsigned char two = second_seat(), s, k, total = 0, row = 0;
    short x = LY.seats_area.x + 4, y = LY.seats_area.y + 2;
    struct rect up = LY.scroll_up, down = LY.scroll_down;

    for (s = 0; s < SEATS; ++s)
        if (s != me && s != two && seat_kind[s] != SEAT_EMPTY)
            ++total;
    if (others_top >= total)
        others_top = total ? total - 1 : 0;
    gfx_bevel(&LY.seats_area, PEN_BG1, PEN_EDGE, PEN_BG0);
    if (me < SEATS)
        table_row(x, y + row++ * 8, me, LY.seats_area.w - 24);
    if (two < SEATS)
        table_row(x, y + row++ * 8, two, LY.seats_area.w - 24);
    for (s = 0, k = 0; s < SEATS; ++s) {
        if (s == me || s == two || seat_kind[s] == SEAT_EMPTY)
            continue;
        if (k++ == others_top) {
            table_row(x, y + row * 8, s, LY.seats_area.w - 24);
            break;
        }
    }
    if (total > 1) {
        gfx_rect(&up, PEN_BG2);
        gfx_text(up.x + 1, up.y + 2, "^", 1, others_top ? PEN_WHITE : PEN_DIM, PEN_BG2);
        gfx_rect(&down, PEN_BG2);
        gfx_text(down.x + 1, down.y + 2, "v", 1, others_top + 1 < total ? PEN_WHITE : PEN_DIM, PEN_BG2);
    }
}

/* While the buy window is up: every netrunner's account and crypto in the table, whatever view the seats are in (PAL: where the
 * seats are; NTSC: taller, where the grid's lower rows were). Closing the window puts the saved picture back. */
void display_buy_table(void)
{
    paint_table_pal(&LY.tbl_buy, 1);
}

void amiga_table_set(unsigned char on)
{
#ifndef TABLEVIEW
    table_view = on;
#endif
}

unsigned char amiga_title_up(void)
{
    return title_up && !field;
}

void amiga_title_leave(void)
{
    title_up = 0;
    menus_update();
}

unsigned char amiga_in_play(void)
{
    return field;
}

unsigned char amiga_helping(void)
{
    return helping;
}

unsigned char amiga_table_view(void)
{
    return table_view;
}

/* F4 or a click on the seats: cards or table. Not while a window or the help covers them (their saved picture would come back). */
void amiga_toggle_table(void)
{
    if (!field || helping || dialog_up())
        return;
    table_view ^= 1;
    if (LY.ntsc)
        paint_seat_area_ntsc();
    else
        paint_seats_pal();
}

void display_kit_stale(void)
{
    kit_valid = 0;
}

unsigned char amiga_scroll_others(signed char d)
{
    if (!LY.ntsc || !field || helping || dialog_up())
        return 0;                       /* not on screen: the click is the window's */
    if (d < 0 && !others_top)
        return 1;
    if (table_view && d > 0 && others_top + 1 >= others_count())
        return 1;
    others_top += d;
    paint_seat_area_ntsc();
    return 1;
}

void display_seat(unsigned char seat)
{
    if (seat >= SEATS)
        return;
    dirty_seat |= 1 << seat;
}

static void seats_now(void)
{
    unsigned char i;
    if (LY.ntsc) {
        paint_seat_area_ntsc();
        return;
    }
    if (table_view) {
        paint_table_pal(&LY.seats_area, 1);             /* the whole box each time: a shorter amount must not leave the longer one's yen behind */
        return;
    }
    for (i = 0; i < SEATS; ++i)
        if (dirty_seat >> i & 1)
            card(&LY.seat[i], i);
}

/* ---------------------------------------------------------------- the log and the prompt strip */

static void paint_log(void)
{
    unsigned char k;
    for (k = 0; k < 3; ++k)
        say(6, LY.log.y + k * 8, logl[k], logn[k], WIDE_CH, k == 2 ? PEN_TEXT : PEN_DIM, PEN_BG0);
}

void display_log(void)
{
    unsigned char n = msg[1];
    memcpy(logl[0], logl[1], logn[1]);
    logn[0] = logn[1];
    memcpy(logl[1], logl[2], logn[2]);
    logn[1] = logn[2];
    if (n > WIDE_CH)
        n = WIDE_CH;
    memcpy(logl[2], msg + 2, n);
    logn[2] = n;
    dirty_log = 1;
}

/* the strip: one line of `width` chars from x = 6 */
static void strip(const char *a, unsigned char n, unsigned char width, unsigned char pen)
{
    say(6, LY.prompt.y + 1, a, n, width, pen, PEN_BG2);
}

static char sb[WIDE_CH + 2];
static unsigned char sn;
static void sput(const char *s, unsigned char n)
{
    while (n-- && sn < WIDE_CH)
        sb[sn++] = *s++;
}
#define SLIT(s) sput(s, sizeof s - 1)

static void clear_clock(void)
{
    struct rect r;
    rect_set(&r, LY.clock.x - 2, LY.prompt.y + 1, 320 - LY.clock.x + 2 - 1, LY.prompt.h - 2);
    gfx_rect(&r, PEN_BG2);
}

void display_prompt(void)
{
    dirty_prompt = 1;
}

static void prompt_now(void)
{
    unsigned char mine = pr_kind || leaving, want;
    const char *ask = 0;

    dialog_sync();                      /* the window this prompt calls for, or the playfield again */
    if (!dialog_up())
        draw_kit(0);                    /* never over a window; display_kit_stale makes it catch up on close */
    want = pr_kind == 'F' ? pr[1] : pr_kind == 'K' ? pr[2] : 0;
    if (field && want != marked)
        dirty_hives = 1;
    sn = 0;
    if (leaving) {                      /* Esc: the question waits; the prompt comes back after */
        ask = T_LOGOFF " NOW? Y/N";
    } else {
        switch (pr_kind) {
        case 0:
            if (turn < SEATS && turn != me) {
                unsigned char l;
                char nm[8];
                SLIT("WAITING FOR ");
                trimmed_name(turn, nm, &l);
                sput(nm, l);
            }
            break;
        case 'S': ask = dialog_holding() ? "GAME OVER. PRESS A KEY" : "START WITH HOW MANY SEATS? 2-6"; break;
        case 'P': ask = "RUN WHICH " T_EXPLOIT "? 1-6"; break;
        case 'F': ask = "PICK A FREE SYNDICATE"; break;
        case 'K': ask = pr[1] ? "TIE! ABSORB WHICH NEXT?" : "TIE! WHICH SURVIVES?"; break;
        case 'D':
            ask = step ? T_SWAP " 2 FOR 1, HOW MANY? " : T_CASHOUT " HOW MANY? ";
            break;
        case 'B':
            ask = step ? T_NULLROUTE ": END GAME? Y/N" : "BUY UP TO 3 " T_COIN ": ";
            break;
        }
    }
    if (complaint)
        ask = complaint;
    if (ask) {
        sput(ask, strlen(ask));
        if (!complaint && !leaving && (pr_kind == 'D' || (pr_kind == 'B' && !step)))
            sput((const char *)edit, edit_len);
    }
    if (!pr_kind || leaving)
        clear_clock();
    strip(sb, sn, !pr_kind || leaving ? WIDE_CH : STRIP_CH, complaint ? PEN_AMBER : mine ? PEN_WHITE : PEN_TEXT);
}

/* One second short, as on the DOS client: the clock ticks a few percent slow, so shown short it never promises more
 * time than the server gives. */
void display_clock(unsigned char secs)
{
    unsigned char total = pr[0] ? pr[0] : 60, shown = secs ? secs - 1 : 0, l;
    struct rect bar, fill;
    if (!pr_kind || helping)
        return;
    bar = LY.clock;
    gfx_rect(&bar, PEN_BG0);
    rect_set(&fill, bar.x + 1, bar.y + 1, (unsigned short)62 * (shown > total ? total : shown) / total, bar.h - 2);
    gfx_rect(&fill, shown > 15 ? PEN_GREEN : PEN_RED);
    l = shown ? number(shown, 0) : 0;
    say(LY.clock.x + 66, LY.prompt.y + 1, numtxt, l, 3, PEN_AMBER, PEN_BG2);
}

void display_message(const char *s)
{
    menus_update();                     /* a dial or a redial turns the menus off, an answer back on */
    if (helping)                        /* the instructions cover the screen; closing them repaints it */
        return;
    gfx_bevel(&LY.prompt, PEN_BG2, PEN_EDGE, PEN_BG0);
    strip(s, strlen(s), WIDE_CH, PEN_AMBER);
#ifdef QUIT_ASK
    if (!field && !strcmp(s, QUIT_ASK))     /* the title's: Y quits, and the window has buttons for the mouse */
        dialog_quit_ask(1);
#endif
}

void display_game_over(void)
{
    unsigned char i, l;
    char nm[8];
    if (helping)                        /* the prompt strip is under the page on NTSC */
        return;
    sn = 0;
    if (winners & (winners - 1))        /* a tie: the names need the room */
        SLIT("TOP " T_PLAYER_P ":");
    else
        SLIT("GAME OVER. TOP " T_PLAYER ":");
    for (i = 0; i < SEATS; ++i)
        if (winners >> i & 1) {
            sput(" ", 1);
            trimmed_name(i, nm, &l);
            sput(nm, l);
        }
    clear_clock();
    strip(sb, sn, WIDE_CH, PEN_WHITE);
    if (pr_kind == 'S' || !pr_kind)
        dialog_hold(1);                 /* the replay's seat window would cover the scores at once */
    over_t = ticks();
}

void amiga_over_release(void)
{
    dialog_hold(0);
    display_prompt();
}

void display_link(void)
{
    struct rect r;
    short x;
    menus_update();
    if (!field || helping)              /* still on the title screen, or the page covers the header */
        return;
    rect_set(&r, 112, 0, 208, 10);
    gfx_rect(&r, PEN_BG2);
    x = 314 - (region_len + 1 + 9) * 8;
    say(x, 1, (const char *)region, region_len, region_len, PEN_GREEN, PEN_BG2);
    gfx_text(x + region_len * 8, 1, ":", 1, PEN_GREEN, PEN_BG2);
    gfx_text(x + (region_len + 1) * 8, 1, link_up ? "CONNECTED" : "NO SIGNAL", 9, link_up ? PEN_GREEN : PEN_RED, PEN_BG2);
}

/* ---------------------------------------------------------------- the whole frame */

static void paint_all(void)
{
    unsigned char i;
    struct rect screen, t;
    dialog_forget();
    rect_set(&screen, 0, 0, LY.width, LY.height);
    gfx_rect(&screen, PEN_BG0);
    gfx_rect(&LY.header, PEN_BG2);
    rect_set(&t, 0, 10, 320, 1);
    gfx_rect(&t, PEN_EDGE);
    gfx_text(6, 1, TH_TITLE, sizeof TH_TITLE - 1, PEN_WHITE, PEN_BG2);
    display_link();
    paint_grid();
    ice_mask = 0;
    hives_now();
    kit_valid = 0;
    draw_kit(1);
    if (LY.ntsc)
        paint_seat_area_ntsc();
    else
        paint_seats_pal();
    paint_log();
    gfx_bevel(&LY.prompt, PEN_BG2, PEN_EDGE, PEN_BG0);
    for (i = 0; i < sizeof dirty_node; ++i)
        dirty_node[i] = 0;
    dirty_seat = dirty_hives = dirty_log = 0;
    prompt_now();                       /* may open a dialog over the repainted playfield */
}

void display_repaint(void)
{
    if (field)
        paint_all();
}

void display_playfield(void)
{
    if (!field) {                       /* a new game: the log starts empty */
        logn[0] = logn[1] = logn[2] = 0;
        others_top = 0;
        blink = done = NONE;
    }
    field = 1;
    gfx_cycle_end();                    /* the playfield's pens are the palette as loaded */
    if (helping)                        /* the page is up: closing it paints the playfield */
        return;
    ui_title_mode(0);
    menus_update();
    paint_all();
}

/* ---------------------------------------------------------------- the title */

/* mockup 1: the name in Topaz glyphs three times the size, a colour each (palette cycling comes in step 3), over circuit
 * traces; the region and status panel; the handle in a string gadget; a row of buttons. */
#define PANEL_X 12
#define VAL_X   76                      /* the panel's and the handle's values */

static void title_button(unsigned char btn, const char *label, unsigned char n, unsigned char fill, unsigned char hi, unsigned char pen)
{
    const struct rect *r = &LY.btn[btn];
    gfx_bevel(r, fill, hi, PEN_BG0);
    gfx_text(r->x + (r->w - n * 8) / 2, r->y + 5, label, n, pen, fill);
}

#define TBUTTON(btn, s, fill, hi, pen) title_button(btn, s, sizeof s - 1, fill, hi, pen)

static void title_buttons(void)
{
    TBUTTON(BTN_JACK, T_JACKIN, edit_len ? PEN_CYAN : PEN_BG1, edit_len ? PEN_WHITE : PEN_EDGE, edit_len ? PEN_BG0 : PEN_DIM);
    TBUTTON(BTN_HINTS, "HINTS", hints ? PEN_BG2 : PEN_BG1, PEN_EDGEHI, hints ? PEN_AMBER : PEN_DIM);
    TBUTTON(BTN_SOUND, "SOUND", PEN_BG2, PEN_EDGEHI, PEN_WHITE);
    TBUTTON(BTN_HELP, "HELP", PEN_BG2, PEN_EDGEHI, PEN_WHITE);
    TBUTTON(BTN_QUIT, "QUIT", PEN_BG2, PEN_EDGEHI, PEN_WHITE);
}

void display_title_update(void)
{
    char b[8];
    unsigned char i, n = 0;
    struct rect cur;

    if (helping)
        return;
    say(VAL_X, 99, (const char *)region, region_len, 12, PEN_WHITE, PEN_BG1);
    if (status_len) {
        n = status_len - 1;
        say(VAL_X, 111, (const char *)status_text, n, 28, PEN_GREEN, PEN_BG1);
    } else if (status_code == NO_CARRIER)
        say(VAL_X, 111, "NO CARRIER", 10, 28, PEN_RED, PEN_BG1);
    else
        say(VAL_X, 111, "", 0, 28, PEN_GREEN, PEN_BG1);
    for (i = 0; i < 8; ++i)
        b[i] = i < edit_len ? edit[i] : ' ';
    rect_set(&cur, VAL_X + 3, 141, 74, 10);     /* inside the frame: the cursor block is taller than the text */
    gfx_rect(&cur, PEN_BG0);
    gfx_text(VAL_X + 6, 142, b, 8, PEN_WHITE, PEN_BG0);
    rect_set(&cur, VAL_X + 6 + (edit_len < 8 ? edit_len : 7) * 8, 141, 6, 10);
    if (edit_len < 8)
        gfx_rect(&cur, PEN_AMBER);
    title_buttons();
}

#define NEON_PENS 7                     /* PEN_SYN .. PEN_SYN + 6, the picture's logo ramp: turned on the title, never in play */
#define NEON_EVERY 2                    /* ticks between turns */

/* The title's life: the light in the logo's rim runs round it. */
void display_title_idle(void)
{
    static unsigned char last;
    unsigned char now = ticks();

    title_up = 1;
    if (helping || (unsigned char)(now - last) < NEON_EVERY)
        return;
    last = now;
    gfx_cycle(PEN_SYN, NEON_PENS, 1);
}

void display_title_screen(void)
{
    struct rect p;

    field = 0;
    ui_title_mode(1);
    dialog_quit_ask(0);
    gfx_art(title_art, title_art_len, title_art_palette);      /* the picture: logo, skyline, floor; the rest goes on top */
    rect_set(&p, PANEL_X, 94, 296, 36);
    gfx_bevel(&p, PEN_BG1, PEN_EDGE, PEN_BG0);
    gfx_text(PANEL_X + 8, 99, "REGION", 6, PEN_DIM, PEN_BG1);
    gfx_text(PANEL_X + 8, 111, "STATUS", 6, PEN_DIM, PEN_BG1);
    gfx_text(PANEL_X + 8, 142, "HANDLE", 6, PEN_DIM, PEN_BG0);
    rect_set(&p, VAL_X, 138, 80, 16);   /* the string gadget */
    gfx_bevel(&p, PEN_BG0, PEN_BG0, PEN_EDGEHI);
    rect_set(&p, VAL_X + 2, 140, 76, 12);
    gfx_frame(&p, PEN_EDGE);
    gfx_bevel(&LY.prompt, PEN_BG2, PEN_EDGE, PEN_BG0);
    if (refused)
        display_message(refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN" : "THAT HANDLE IS TAKEN");
    else
        display_message("TYPE A HANDLE, THEN RETURN. ESC QUITS");
    if (!LY.ntsc)                       /* in the dark band above the floor */
        gfx_text(12, 194, "FUJINET RS-232", 14, PEN_STEEL, PEN_BG0);
    display_title_update();
}

/* ---------------------------------------------------------------- the instructions */

static unsigned char help_pg;

void help_open(void)
{
    helping = 1;
    gfx_cycle_end();
    menus_update();
    help_pg = 1;
    ui_help_mode(1);
    help_draw(help_pg);
}

unsigned char help_key(unsigned char k)
{
    if (k == '>' || k == '<') {
        if (k == '>' && help_pg < help_pages())
            help_draw(++help_pg);
        else if (k == '<' && help_pg > 1)
            help_draw(--help_pg);
        return 1;
    }
    if (k != K_ESC && k != K_RETURN && k != K_HELP)
        return 1;
    helping = 0;
    menus_update();
    ui_help_mode(0);
    if (field) {
        ui_title_mode(0);
        paint_all();
    } else
        display_title_screen();
    return 0;
}
