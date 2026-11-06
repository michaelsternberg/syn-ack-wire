/* The dialogs, drawn over the playfield dimmed by a checkerboard: the buy window, the hivejack and seat-count windows, and
 * the Y/N question. Each is drawn whole from the game state (game.h) every time dialog_sync() is called, and closing one
 * repaints the playfield, which is simple and exact. The windows are the mockups'; layout.c has their rectangles. */
#include <string.h>
#include "../platform.h"
#include "../common/game.h"
#include "../common/proto_tab.h"
#include "amiga.h"
#include "gfx.h"
#include "ui.h"

#define LY amiga_layout

static const char *const syn_short[NSYN] = SYN_SHORTS;
static char shown;                      /* 'B', 'Q' (a question), 'D', 'S', or 0 */
static unsigned char busy;
static unsigned char saved;             /* the playfield under the window is kept aside, to put back */
static unsigned char over_hold;         /* the game just ended: the seat-count window waits for a key, the scores stay in view */
static unsigned char quit_q;            /* the title's "quit?" question is up */

unsigned char dialog_quit_open(void)
{
    return quit_q;
}

unsigned char dialog_holding(void)
{
    return over_hold;
}

void dialog_hold(unsigned char on)
{
    over_hold = on;
}

unsigned char dialog_up(void)
{
    return shown != 0;
}

/* The whole screen was repainted from the game state: what was saved under the window is out of date, so drop it
 * unblitted; dialog_sync then opens the window again over the fresh paint. */
void dialog_forget(void)
{
    if (!shown || busy)
        return;
    gfx_discard();
    saved = 0;
    shown = 0;
}

/* which window the game state calls for */
static char wanted(void)
{
    if (leaving)
        return 'Q';
    switch (pr_kind) {
    case 'B': return step ? 'Q' : 'B';
    case 'D': return 'D';
    case 'S': return over_hold ? 0 : 'S';
    }
    return 0;
}

static char numtxt[12];
static unsigned char number(unsigned long v, unsigned char money)
{
    char t[12];
    unsigned char n = 0, k = 0;
    if (money && v)
        v *= 100;
    do {
        t[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    if (money)
        numtxt[k++] = 0xA5;
    while (n)
        numtxt[k++] = t[--n];
    return k;
}

/* numtxt right-aligned in `width` chars at x */
static void num_at(short x, short y, unsigned char l, unsigned char width, unsigned char pen, unsigned char bg)
{
    gfx_text(x + (width - l) * 8, y, numtxt, l, pen, bg);
}

static void window(const struct rect *r, const char *title, unsigned char n)
{
    struct rect shadow = *r, bar;
    shadow.x += 3;
    shadow.y += 3;
    gfx_rect(&shadow, PEN_BG0);
    gfx_bevel(r, PEN_BG2, PEN_EDGEHI, PEN_BG0);
    bar.x = r->x + 1; bar.y = r->y + 1; bar.w = r->w - 2; bar.h = 11;
    gfx_rect(&bar, PEN_CYAN);
    gfx_text(r->x + 6, r->y + 2, title, n, PEN_BG0, PEN_CYAN);
}

static void button(const struct rect *r, const char *label, unsigned char n, unsigned char fill, unsigned char hi, unsigned char pen)
{
    gfx_bevel(r, fill, hi, PEN_BG0);
    gfx_text(r->x + (r->w - n * 8) / 2, r->y + 4, label, n, pen, fill);
}

/* a line under the first letter of a button's label: the key that presses it */
static void underline(const struct rect *r, unsigned char n, unsigned char pen)
{
    struct rect u;
    u.x = r->x + (r->w - n * 8) / 2; u.y = r->y + 12; u.w = 8; u.h = 1;
    gfx_rect(&u, pen);
}

#define LABEL(r, s, fill, hi, pen) button(r, s, sizeof s - 1, fill, hi, pen)
#define TITLE(r, s) window(r, s, sizeof s - 1)

static void draw_buy(void)
{
    const struct rect *w = &LY.dlg;
    unsigned char i, k, n, l, off = LY.ntsc ? 1 : 2, bought[NSYN], hy = LY.ntsc ? 13 : 14;
    unsigned total = 0, cash_here = pr[1] | (unsigned)pr[2] << 8;

    TITLE(w, "BUY " T_COIN);
    gfx_text(w->x + 18, w->y + hy, T_HIVE, sizeof T_HIVE - 1, PEN_DIM, PEN_BG2);
    gfx_text(w->x + 70, w->y + hy, "HELD", 4, PEN_DIM, PEN_BG2);   /* what the netrunner already has: the choice needs it */
    gfx_text(w->x + 108, w->y + hy, "PRICE", 5, PEN_DIM, PEN_BG2);
    gfx_text(w->x + 152, w->y + hy, "LEFT", 4, PEN_DIM, PEN_BG2);
    memset(bought, 0, sizeof bought);
    for (i = 0; i < edit_len; ++i)
        for (k = 0; k < NSYN; ++k)
            if (edit[i] == syn_letter[k]) {
                ++bought[k];
                total += pr[4 + k];
            }
    for (i = 0; i < NSYN; ++i) {
        const struct rect *row = &LY.dlg_row[i];
        unsigned char on = pr[4 + i] != 0, y = row->y + off, bg = i & 1 ? PEN_BG2 : PEN_BG1;
        struct rect sw = { w->x + 8, y, 8, LY.ntsc ? 8 : 9 };
        gfx_rect(row, bg);
        gfx_rect(&sw, on ? PEN_SYN + i : PEN_SYN_DIM + i);
        gfx_text(sw.x, y, &syn_letter[i], 1, PEN_BG0, on ? PEN_SYN + i : PEN_SYN_DIM + i);   /* the key that buys it */
        gfx_text(w->x + 20, y, syn_short[i], strlen(syn_short[i]), on ? PEN_WHITE : PEN_DIM, bg);
        if (me < SEATS && coins[me][i]) {
            l = number(coins[me][i], 0);
            num_at(w->x + 86, y, l, 2, PEN_SYN + i, bg);
        } else {
            gfx_text(w->x + 102, y, ".", 1, PEN_DIM, bg);
        }
        if (on) {
            l = number(pr[4 + i], 1);
            num_at(w->x + 108, y, l, 5, PEN_AMBER, bg);
            l = number(pr[11 + i], 0);
            num_at(w->x + 152, y, l, 2, PEN_TEXT, bg);
        } else {
            gfx_text(w->x + 108, y, "  ---", 5, PEN_DIM, bg);
        }
        for (n = 0; n < bought[i]; ++n) {
            struct rect chip = { w->x + 172 + n * 14, row->y, 12, row->h - 1 };
            char c = syn_letter[i];
            gfx_bevel(&chip, PEN_SYN + i, PEN_WHITE, PEN_BG0);
            gfx_text(chip.x + 2, chip.y + (chip.h - 7) / 2, &c, 1, PEN_BG0, PEN_SYN + i);
        }
    }
    {                                   /* the tally, above the buttons in the column at the right */
        short x = w->x + 220, y = w->y + (LY.ntsc ? 14 : 16), dy = LY.ntsc ? 9 : 11;
        unsigned char over = total > cash_here;
        gfx_text(x, y, "TOTAL", 5, PEN_WHITE, PEN_BG2);
        l = number(total, 1);
        gfx_text_right(x + 88, y, numtxt, l, PEN_WHITE, PEN_BG2);
        gfx_text(x, y + dy, "CASH", 4, PEN_TEXT, PEN_BG2);
        l = number(cash_here, 1);
        gfx_text_right(x + 88, y + dy, numtxt, l, PEN_TEXT, PEN_BG2);
        gfx_text(x, y + 2 * dy, "LEFT", 4, over ? PEN_RED : PEN_GREEN, PEN_BG2);
        l = number(over ? 0 : cash_here - total, 1);
        gfx_text_right(x + 88, y + 2 * dy, numtxt, l, over ? PEN_RED : PEN_GREEN, PEN_BG2);
    }
    LABEL(&LY.btn[BTN_BUY], "BUY", PEN_GREEN, PEN_WHITE, PEN_BG0);
    LABEL(&LY.btn[BTN_UNDO], "UNDO", PEN_BG2, PEN_EDGEHI, PEN_WHITE);
    button(&LY.btn[BTN_NULLROUTE], T_NULLROUTE, sizeof T_NULLROUTE - 1, PEN_BG1, PEN_EDGEHI, pr[3] & 1 ? PEN_WHITE : PEN_DIM);
}

static void draw_question(void)
{
    const struct rect *w = &LY.ask;
    if (quit_q) {
        window(w, "QUIT", 4);
        gfx_text(w->x + 8, w->y + 20, "QUIT " TH_NAME "?", sizeof TH_NAME + 5, PEN_WHITE, PEN_BG2);
    } else if (leaving) {
        window(w, T_LOGOFF, sizeof T_LOGOFF - 1);
        gfx_text(w->x + 8, w->y + 20, T_LOGOFF " NOW?", sizeof T_LOGOFF + 4, PEN_WHITE, PEN_BG2);
    } else {
        window(w, T_NULLROUTE, sizeof T_NULLROUTE - 1);
        gfx_text(w->x + 8, w->y + 20, "END THE GAME?", 13, PEN_WHITE, PEN_BG2);
    }
    LABEL(&LY.btn[BTN_YES], "YES", PEN_GREEN, PEN_WHITE, PEN_BG0);
    LABEL(&LY.btn[BTN_NO], "NO", PEN_BG2, PEN_EDGEHI, PEN_WHITE);
    underline(&LY.btn[BTN_YES], 3, PEN_BG0);        /* the key that answers: Y or N */
    underline(&LY.btn[BTN_NO], 2, PEN_WHITE);
}

static void draw_hivejack(void)
{
    const struct rect *w = &LY.dlg_small;
    unsigned char l;
    char c;
    TITLE(w, T_HIVEJACK);
    c = syn_letter[pr[2]];
    gfx_text(w->x + 8, w->y + 16, &c, 1, PEN_SYN + pr[2], PEN_BG2);
    gfx_text(w->x + 16, w->y + 16, " TAKES ", 7, PEN_TEXT, PEN_BG2);
    c = syn_letter[pr[1]];
    gfx_text(w->x + 72, w->y + 16, &c, 1, PEN_SYN + pr[1], PEN_BG2);
    gfx_text(w->x + 80, w->y + 16, ". YOU HOLD ", 11, PEN_TEXT, PEN_BG2);
    l = number(pr[3], 0);
    gfx_text(w->x + 168, w->y + 16, numtxt, l, PEN_WHITE, PEN_BG2);
    gfx_text(w->x + 168 + l * 8 + 8, w->y + 16, &c, 1, PEN_SYN + pr[1], PEN_BG2);
    if (step)
        gfx_text(w->x + 8, w->y + 28, T_SWAP " 2 FOR 1, HOW MANY?", sizeof T_SWAP + 18, PEN_AMBER, PEN_BG2);
    else
        gfx_text(w->x + 8, w->y + 28, T_CASHOUT ", HOW MANY?", sizeof T_CASHOUT + 10, PEN_AMBER, PEN_BG2);
    LABEL(&LY.btn[BTN_MINUS], "-", PEN_BG1, PEN_EDGEHI, PEN_WHITE);
    LABEL(&LY.btn[BTN_PLUS], "+", PEN_BG1, PEN_EDGEHI, PEN_WHITE);
    if (edit_len)                       /* typed on the keyboard, not stepped */
        memcpy(numtxt, edit, l = edit_len);
    else
        l = number(ui_count(), 0);
    gfx_text(w->x + 100, w->y + 40, "   ", 3, PEN_WHITE, PEN_BG2);
    gfx_text(w->x + 112 - l * 4, w->y + 40, numtxt, l, PEN_WHITE, PEN_BG2);
    LABEL(&LY.btn[BTN_OK], "OK", PEN_GREEN, PEN_WHITE, PEN_BG0);
    if (complaint)
        gfx_text(w->x + 8, w->y + 98, complaint, strlen(complaint) > 27 ? 27 : strlen(complaint), PEN_AMBER, PEN_BG2);
}

static void draw_seats(void)
{
    const struct rect *w = &LY.dlg_small;
    unsigned char i;
    char c;
    TITLE(w, "SEATS");
    gfx_text(w->x + 10, w->y + 24, "START WITH HOW MANY SEATS?", 26, PEN_WHITE, PEN_BG2);
    for (i = 0; i < 5; ++i) {
        const struct rect *b = &LY.btn[BTN_SEAT2 + i];
        c = '2' + i;
        gfx_bevel(b, PEN_BG1, PEN_EDGEHI, PEN_BG0);
        gfx_text(b->x + 14, b->y + 4, &c, 1, PEN_CYAN, PEN_BG1);
    }
}

static void draw_window(char kind)
{
    if (kind == 'B')
        draw_buy();
    else if (kind == 'Q')
        draw_question();
    else if (kind == 'D')
        draw_hivejack();
    else if (kind == 'S')
        draw_seats();
}

/* Called whenever the game state may have changed what is asked: opens the window it calls for over the dimmed
 * playfield, changes one window for another by repainting first, redraws the one that stays, or closes it. */
void dialog_sync(void)
{
    char want;
    struct rect below;
    if (busy)
        return;
    busy = 1;
    want = wanted();
    if (want != shown) {
        if (shown) {
            shown = 0;
            if (saved) {                /* what was behind it: put back, and what changed meanwhile is painted next */
                gfx_restore();
                saved = 0;
                display_kit_stale();    /* the restored picture has the kit panel as it was */
            } else
                display_repaint();      /* no memory to keep it in: from the game state */
        }
        if (want) {
            below.x = 0; below.y = LY.header.h; below.w = LY.width; below.h = LY.prompt.y - LY.header.h;
            if (want == 'B') {          /* the table and the log stay in view: only what is above the table is dimmed */
                below.h = LY.tbl_buy.y + LY.tbl_buy.h - LY.header.h;
                saved = gfx_save(&below);
                below.h = LY.tbl_buy.y - LY.header.h;
                gfx_dim(&below, PEN_BG0);
                display_buy_table();
            } else {
                saved = gfx_save(&below);
                gfx_dim(&below, PEN_BG0);
            }
            ui_reset();
        }
        shown = want;
    }
    if (shown)
        draw_window(shown);
    busy = 0;
    menus_update();
}

/* The title's quit question: Y quits and anything else is a no, by key or by click (ui_ask_yes_no). */
void dialog_quit_ask(unsigned char on)
{
    struct rect below;
    quit_q = on;
    ui_ask_yes_no(on);
    menus_update();
    if (!on)
        return;
    below.x = 0; below.y = LY.header.h; below.w = LY.width; below.h = LY.prompt.y - LY.header.h;
    gfx_dim(&below, PEN_BG0);
    draw_question();
}
