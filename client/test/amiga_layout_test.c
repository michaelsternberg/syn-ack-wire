/* Host tests for src/amiga/layout.c: rectangles for PAL and NTSC, and which widget a pixel is on.
 *   make test-amiga-layout */
#include <stdio.h>
#include <string.h>
#include "../src/amiga/layout.h"

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static const unsigned short heights[2] = { 256, 200 };

static int inside_screen(const struct layout *l, const struct rect *r)
{
    return r->w == 0 || (r->x >= 0 && r->y >= 0 && r->x + r->w <= l->width && r->y + r->h <= l->height);
}
static int overlap(const struct rect *a, const struct rect *b)
{
    return a->w && b->w && a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static void test_every_rect_within_screen(void)
{
    int k, i, n;
    struct layout l;
    for (k = 0; k < 2; ++k) {
        layout_make(&l, heights[k]);
        CHECK(l.width == 320 && l.height == heights[k]);
        CHECK(inside_screen(&l, &l.header) && inside_screen(&l, &l.grid) && inside_screen(&l, &l.syn_panel));
        CHECK(inside_screen(&l, &l.kit_panel) && inside_screen(&l, &l.log) && inside_screen(&l, &l.prompt));
        CHECK(inside_screen(&l, &l.clock) && inside_screen(&l, &l.others));
        for (i = 0; i < NSYN; ++i) CHECK(inside_screen(&l, &l.syn_row[i]));
        for (i = 0; i < RULE_KITSIZE; ++i) CHECK(inside_screen(&l, &l.kit_chip[i]));
        for (i = 0; i < RULE_MAXPLAYERS; ++i) CHECK(inside_screen(&l, &l.seat[i]));
        for (n = 0; n < RULE_ROWS * RULE_COLS; ++n) {
            struct rect r;
            layout_node_rect(&l, n, &r);
            CHECK(inside_screen(&l, &r));
        }
    }
}
static void test_no_overlap_between_panels(void)
{
    int k, i, j;
    struct layout l;
    for (k = 0; k < 2; ++k) {
        const struct rect *p[16];
        int n = 0;
        layout_make(&l, heights[k]);
        p[n++] = &l.header; p[n++] = &l.grid; p[n++] = &l.syn_panel; p[n++] = &l.kit_panel; p[n++] = &l.log;
        p[n++] = &l.prompt;
        for (i = 0; i < l.seat_cards; ++i) p[n++] = &l.seat[i];
        if (l.ntsc) p[n++] = &l.others;
        for (i = 0; i < n; ++i)
            for (j = i + 1; j < n; ++j)
                CHECK(!overlap(p[i], p[j]));
    }
}
static void test_node_roundtrip(void)
{
    int k, n;
    struct layout l;
    for (k = 0; k < 2; ++k) {
        layout_make(&l, heights[k]);
        for (n = 0; n < RULE_ROWS * RULE_COLS; ++n) {
            struct rect r;
            layout_node_rect(&l, n, &r);
            CHECK(layout_node_at(&l, r.x + r.w / 2, r.y + r.h / 2) == n);
        }
    }
}
static void test_node_borders_half_open(void)
{
    int k;
    struct layout l;
    for (k = 0; k < 2; ++k) {
        struct rect a, b;
        layout_make(&l, heights[k]);
        layout_node_rect(&l, 0, &a);
        layout_node_rect(&l, 1, &b);
        CHECK(layout_node_at(&l, a.x, a.y) == 0);
        CHECK(layout_node_at(&l, a.x + a.w - 1, a.y + a.h - 1) == 0);
        CHECK(layout_node_at(&l, b.x, a.y) == 1);                       /* the next pixel is the neighbour */
        CHECK(layout_node_at(&l, a.x - 1, a.y) == -1);
        CHECK(layout_node_at(&l, a.x, a.y - 1) == -1);
        layout_node_rect(&l, RULE_ROWS * RULE_COLS - 1, &b);
        CHECK(layout_node_at(&l, b.x + b.w, b.y) == -1);
        CHECK(layout_node_at(&l, b.x, b.y + b.h) == -1);
    }
}
static void test_gaps_are_not_nodes(void)
{
    struct layout l;
    int k;
    for (k = 0; k < 2; ++k) {
        layout_make(&l, heights[k]);
        CHECK(layout_node_at(&l, l.grid_x - 4, l.grid_y + 3) == -1);    /* the row numbers' column */
        CHECK(layout_node_at(&l, l.grid_x + 3, l.grid.y + 2) == -1);    /* the column letters' row, above the nodes */
        CHECK(layout_node_at(&l, -1, -1) == -1);
        CHECK(layout_node_at(&l, 320, 255) == -1);
        CHECK(layout_node_at(&l, 10000, 10000) == -1);
    }
}
static void test_syn_and_kit_hits_roundtrip(void)
{
    struct layout l;
    int k, i;
    for (k = 0; k < 2; ++k) {
        layout_make(&l, heights[k]);
        for (i = 0; i < NSYN; ++i)
            CHECK(layout_syn_at(&l, l.syn_row[i].x + 2, l.syn_row[i].y + 2) == i);
        for (i = 0; i < RULE_KITSIZE; ++i)
            CHECK(layout_kit_at(&l, l.kit_chip[i].x + 2, l.kit_chip[i].y + 2) == i);
        CHECK(layout_syn_at(&l, 0, 0) == -1 && layout_kit_at(&l, 0, 0) == -1);
    }
}
static void test_help_page_and_its_footer_zones_fit(void)
{
    struct layout p, n;
    const struct layout *l[2] = { &p, &n };
    int i;
    layout_make(&p, 256);
    layout_make(&n, 200);
    for (i = 0; i < 2; ++i) {
        const struct rect *pg = &l[i]->help_page, *z[3] = { &l[i]->help_prev, &l[i]->help_exit, &l[i]->help_next };
        int k;
        CHECK(pg->w == 320 && pg->h == 192 && pg->y >= 0 && pg->y + pg->h <= l[i]->height);
        for (k = 0; k < 3; ++k) {
            CHECK(z[k]->w > 0 && z[k]->x >= pg->x && z[k]->x + z[k]->w <= pg->x + pg->w);
            CHECK(z[k]->y + z[k]->h == pg->y + pg->h);                  /* they end with the page's footer row */
        }
        CHECK(z[0]->x + z[0]->w <= z[1]->x && z[1]->x + z[1]->w <= z[2]->x);   /* disjoint, left to right */
    }
}
static void test_question_buttons_sit_below_its_text(void)
{
    struct layout p, n;
    const struct layout *l[2] = { &p, &n };
    int i;
    layout_make(&p, 256);
    layout_make(&n, 200);
    for (i = 0; i < 2; ++i) {
        const struct rect *w = &l[i]->ask, *y = &l[i]->btn[BTN_YES], *no = &l[i]->btn[BTN_NO];
        CHECK(y->y >= w->y + 20 + 8 && no->y >= w->y + 20 + 8);          /* the question is a line at +20, 8 high */
        CHECK(y->y + y->h <= w->y + w->h && no->y + no->h <= w->y + w->h);
    }
}
static void test_seats_area_holds_every_seat_widget(void)
{
    struct layout p, n;
    int i;
    layout_make(&p, 256);
    layout_make(&n, 200);
    for (i = 0; i < p.seat_cards; ++i)
        CHECK(p.seat[i].x >= p.seats_area.x && p.seat[i].y >= p.seats_area.y &&
              p.seat[i].x + p.seat[i].w <= p.seats_area.x + p.seats_area.w && p.seat[i].y + p.seat[i].h <= p.seats_area.y + p.seats_area.h);
    CHECK(n.seats_area.y + n.seats_area.h <= n.log.y && p.seats_area.y + p.seats_area.h <= p.log.y);
    CHECK(layout_inside(&n.seats_area, n.scroll_up.x, n.scroll_up.y) && layout_inside(&n.seats_area, n.scroll_down.x, n.scroll_down.y));
    CHECK(p.seats_area.h >= 7 * 8 && n.seats_area.h >= 3 * 8);      /* the table's rows: a header and six, or three */
}
static void test_ntsc_shows_me_and_mover(void)
{
    struct layout l, p;
    layout_make(&l, 200);
    layout_make(&p, 256);
    CHECK(l.ntsc && !p.ntsc);
    CHECK(l.seat_cards == 2 && p.seat_cards == RULE_MAXPLAYERS);
    CHECK(l.others.w > 0 && l.others.y >= l.grid.y + l.grid.h);        /* the list is below the grid */
    CHECK(l.scroll_up.w > 0 && l.scroll_down.w > 0);
    CHECK(layout_seat_at(&l, l.seat[0].x + 3, l.seat[0].y + 3) == -1);   /* NTSC cards are not tied to seat numbers */
    CHECK(layout_seat_at(&p, p.seat[4].x + 3, p.seat[4].y + 3) == 4);
}
static void test_height_clamps(void)
{
    struct layout l;
    int n;
    layout_make(&l, 150);
    CHECK(l.ntsc && l.height <= 200);
    for (n = 0; n < RULE_ROWS * RULE_COLS; ++n) {
        struct rect r;
        layout_node_rect(&l, n, &r);
        CHECK(inside_screen(&l, &r));
    }
    layout_make(&l, 300);
    CHECK(!l.ntsc && l.height == 256);
}
static void test_title_buttons_in_one_row_above_the_prompt(void)
{
    static const unsigned short heights[2] = { 256, 200 };
    int h, a, b;
    for (h = 0; h < 2; ++h) {
        struct layout l;
        layout_make(&l, heights[h]);
        for (a = BTN_JACK; a <= BTN_QUIT; ++a) {
            CHECK(l.btn[a].w > 0 && l.btn[a].x >= 0 && l.btn[a].x + l.btn[a].w <= l.width);
            CHECK(l.btn[a].y + l.btn[a].h <= l.prompt.y);               /* NTSC: not under the strip */
            CHECK(l.btn[a].y == l.btn[BTN_JACK].y);
            for (b = BTN_JACK; b <= BTN_QUIT; ++b)
                if (b != a)
                    CHECK(!(l.btn[a].x < l.btn[b].x + l.btn[b].w && l.btn[b].x < l.btn[a].x + l.btn[a].w));
        }
        CHECK(layout_btn_in(&l, l.btn[BTN_QUIT].x, l.btn[BTN_QUIT].y, BTN_JACK, BTN_QUIT) == BTN_QUIT);
        CHECK(layout_btn_in(&l, l.btn[BTN_QUIT].x, l.btn[BTN_QUIT].y, BTN_BUY, BTN_SEAT6) != BTN_QUIT);   /* a context sees only its own */
        CHECK(layout_btn_at(&l, 0, 0) == -1);
    }
}
static void test_log_holds_three_lines_and_clears_the_rest(void)
{
    static const unsigned short heights[2] = { 256, 200 };
    int h, i;
    for (h = 0; h < 2; ++h) {
        struct layout l;
        layout_make(&l, heights[h]);
        CHECK(l.log.h >= 24);                                   /* a move's spike, spread and three buys */
        CHECK(l.log.y + l.log.h <= l.prompt.y);
        for (i = 0; i < l.seat_cards; ++i)
            CHECK(l.seat[i].y + l.seat[i].h <= l.log.y);
        CHECK(l.grid.y + l.grid.h <= l.seat[0].y);
        CHECK(l.kit_panel.y + l.kit_panel.h <= l.seat[0].y);
        CHECK(l.kit_chip[RULE_KITSIZE - 1].y + l.kit_chip[RULE_KITSIZE - 1].h <= l.kit_panel.y + l.kit_panel.h);
    }
}
static int within(const struct rect *in, const struct rect *out)
{
    return in->x >= out->x && in->y >= out->y && in->x + in->w <= out->x + out->w && in->y + in->h <= out->y + out->h;
}
static int apart(const struct rect *a, const struct rect *b)
{
    return a->x + a->w <= b->x || b->x + b->w <= a->x || a->y + a->h <= b->y || b->y + b->h <= a->y;
}
static void test_dialog_fits_above_prompt(void)
{
    static const unsigned short heights[2] = { 256, 200 };
    int h;
    for (h = 0; h < 2; ++h) {
        struct layout l;
        layout_make(&l, heights[h]);
        CHECK(l.dlg.y >= l.header.y + l.header.h && l.dlg.y + l.dlg.h <= l.prompt.y);
        CHECK(l.dlg.x >= 0 && l.dlg.x + l.dlg.w <= l.width);
        CHECK(l.dlg_small.y >= l.header.y + l.header.h && l.dlg_small.y + l.dlg_small.h <= l.prompt.y);
        CHECK(l.ask.x >= 0 && l.ask.x + l.ask.w <= l.width && l.ask.y + l.ask.h <= l.prompt.y);
        CHECK(l.dlg.w == 312 && l.dlg.h == (h ? 90 : 120));             /* the wide, short buy window */
        CHECK(l.dlg_small.w == 228);                                    /* the mockups' small windows */
        /* the table stays in view under the buy window, and the log is never touched */
        CHECK(l.dlg.y + l.dlg.h <= l.tbl_buy.y && l.tbl_buy.y + l.tbl_buy.h <= l.log.y);
        CHECK(l.tbl_buy.h >= 2 + 8 * (1 + RULE_MAXPLAYERS));            /* a header and every seat's row of 8 */
    }
}
static void test_buttons_inside_dialog_and_disjoint(void)
{
    static const unsigned short heights[2] = { 256, 200 };
    int h, a, b, i;
    for (h = 0; h < 2; ++h) {
        struct layout l;
        layout_make(&l, heights[h]);
        for (a = BTN_BUY; a <= BTN_NULLROUTE; ++a)
            CHECK(within(&l.btn[a], &l.dlg));
        for (a = BTN_OK; a <= BTN_SEAT6; ++a)
            CHECK(within(&l.btn[a], &l.dlg_small));
        CHECK(within(&l.btn[BTN_YES], &l.ask) && within(&l.btn[BTN_NO], &l.ask));
        for (a = BTN_BUY; a <= BTN_SEAT6; ++a) {
            CHECK(l.btn[a].w > 0 && l.btn[a].h > 0);
            for (b = a + 1; b <= BTN_SEAT6; ++b)
                if ((a <= BTN_NULLROUTE) == (b <= BTN_NULLROUTE))     /* the buy window's three, and the rest, are each apart */
                    CHECK(apart(&l.btn[a], &l.btn[b]));                 /* one click, one button, in a dialog's own set */
        }
        for (i = 0; i < NSYN; ++i) {
            CHECK(within(&l.dlg_row[i], &l.dlg));
            CHECK(i == 0 || apart(&l.dlg_row[i - 1], &l.dlg_row[i]));
            CHECK(apart(&l.dlg_row[i], &l.btn[BTN_BUY]) && apart(&l.dlg_row[i], &l.btn[BTN_UNDO]) && apart(&l.dlg_row[i], &l.btn[BTN_NULLROUTE]));
        }
    }
}

int main(void)
{
    RUN(test_every_rect_within_screen); RUN(test_no_overlap_between_panels); RUN(test_node_roundtrip);
    RUN(test_node_borders_half_open); RUN(test_gaps_are_not_nodes); RUN(test_syn_and_kit_hits_roundtrip);
    RUN(test_help_page_and_its_footer_zones_fit); RUN(test_question_buttons_sit_below_its_text); RUN(test_seats_area_holds_every_seat_widget); RUN(test_ntsc_shows_me_and_mover); RUN(test_height_clamps); RUN(test_title_buttons_in_one_row_above_the_prompt);
    RUN(test_log_holds_three_lines_and_clears_the_rest); RUN(test_dialog_fits_above_prompt); RUN(test_buttons_inside_dialog_and_disjoint);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
