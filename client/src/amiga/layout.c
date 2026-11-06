#include "layout.h"

static void set(struct rect *r, short x, short y, short w, short h)
{
    r->x = x; r->y = y; r->w = w; r->h = h;
}

/* The windows of the dialogs. The buy window is the full width and short (the grid and the side panel are not needed while
 * buying), so that the table of netrunners below it stays in view: on PAL it ends at y = 132 above the seats' table (160), on
 * NTSC at 102 above a table the buy window makes taller (106 to 165), the log untouched. Its rows run down the left; the tally
 * and the three buttons stack in the column at the right. The small windows (hivejack, seat count, Y/N) are the mockups'
 * 228 wide ones, where they were. The buy column shares screen with their buttons, so a click looks among its own dialog's
 * (layout_btn_in). */
static void dialogs(struct layout *l, unsigned char ntsc)
{
    short wx = 46, wy = ntsc ? 14 : 22, i;
    short bx = 4, by = 12, bw = 312, bh = ntsc ? 90 : 120, step = ntsc ? 9 : 13, bt = ntsc ? 13 : 16, bg = ntsc ? 14 : 18;

    set(&l->dlg, bx, by, bw, bh);
    for (i = 0; i < NSYN; ++i)
        set(&l->dlg_row[i], bx + 4, by + (ntsc ? 22 : 24) + i * step, 212, step - (ntsc ? 0 : 1));
    set(&l->btn[BTN_BUY], bx + 220, by + (ntsc ? 44 : 56), 88, bt);
    set(&l->btn[BTN_UNDO], bx + 220, by + (ntsc ? 44 : 56) + bg, 88, bt);
    set(&l->btn[BTN_NULLROUTE], bx + 220, by + (ntsc ? 44 : 56) + 2 * bg, 88, bt);
    if (ntsc)
        set(&l->tbl_buy, 0, 106, 320, l->log.y - 106);
    else
        l->tbl_buy = l->seats_area;
    set(&l->dlg_small, wx, wy, 228, 108);
    set(&l->btn[BTN_MINUS], wx + 70, wy + 36, 24, 16);
    set(&l->btn[BTN_PLUS], wx + 134, wy + 36, 24, 16);
    for (i = 0; i < 5; ++i)
        set(&l->btn[BTN_SEAT2 + i], wx + 14 + i * 42, wy + 58, 36, 16);
    set(&l->btn[BTN_OK], wx + 90, wy + 80, 48, 16);
    set(&l->ask, 62, wy + 104, 196, 56);
    set(&l->btn[BTN_YES], l->ask.x + 24, wy + 104 + 32, 36, 16);
    set(&l->btn[BTN_NO], l->ask.x + l->ask.w - 24 - 28, wy + 104 + 32, 28, 16);
}

/* The title's row of buttons, one row on both heights (the NTSC strip starts at 190). */
static void title_buttons(struct layout *l)
{
    static const unsigned char width[5] = { 72, 56, 56, 48, 48 };       /* JACK IN, HINTS, SOUND, HELP, QUIT: 8 a letter + 16 */
    short x = 12, i;
    for (i = 0; i < 5; ++i) {
        set(&l->btn[BTN_JACK + i], x, 168, width[i], 18);
        x += width[i] + 4;
    }
}

/* The numbers are the approved mockups' (tools/amiga_mock.py: draw_grid_A, draw_side, seat_cards, prompt_strip and their NTSC
 * twins). PAL: 256 lines, cells 16x15, a card for every seat. NTSC: 200 lines, cells 16x13, two cards and a list. */
void layout_make(struct layout *l, unsigned short height)
{
    unsigned char i, ntsc = height < 256;
    unsigned char *z = (unsigned char *)l;
    unsigned short k;

    for (k = 0; k < sizeof *l; ++k)
        z[k] = 0;
    l->width = 320;
    l->height = ntsc ? 200 : 256;
    l->ntsc = ntsc;
    l->cell_w = 16;
    l->cell_h = ntsc ? 12 : 15;
    l->grid_x = 14;
    l->grid_y = ntsc ? 21 : 22;
    set(&l->header, 0, 0, 320, 11);
    set(&l->grid, 4, l->grid_y - 10, RULE_COLS * l->cell_w + 12, RULE_ROWS * l->cell_h + 12);
    if (ntsc) {
        set(&l->syn_panel, 218, 12, 98, 75);
        set(&l->kit_panel, 218, 88, 98, 47);
        set(&l->log, 0, 165, 320, 24);
        set(&l->prompt, 0, 190, 320, 10);
        set(&l->clock, 228, 192, 64, 6);
        l->seat_cards = 2;
        set(&l->seat[0], 4, 137, 102, 28);
        set(&l->seat[1], 108, 137, 102, 28);
        set(&l->others, 212, 137, 104, 28);
        set(&l->scroll_up, 304, 139, 9, 12);
        set(&l->scroll_down, 304, 152, 9, 12);
        set(&l->seats_area, 0, 137, 320, 28);
    } else {
        set(&l->syn_panel, 218, 12, 98, 80);
        set(&l->kit_panel, 218, 96, 98, 62);
        set(&l->log, 0, 222, 320, 24);
        set(&l->prompt, 0, 246, 320, 10);
        set(&l->clock, 228, 248, 64, 6);
        l->seat_cards = RULE_MAXPLAYERS;
        for (i = 0; i < RULE_MAXPLAYERS; ++i)
            set(&l->seat[i], 4 + (i % 3) * 105, 161 + (i / 3) * 31, 102, 29);
        set(&l->seats_area, 0, 160, 320, 62);
    }
    set(&l->help_page, 0, (l->height - 192) / 2, 320, 192);          /* 24 rows of 8, the page text's own size */
    set(&l->help_prev, 0, l->help_page.y + 176, 104, 16);            /* the footer row and the one above it */
    set(&l->help_exit, 104, l->help_page.y + 176, 112, 16);
    set(&l->help_next, 216, l->help_page.y + 176, 104, 16);
    dialogs(l, ntsc);
    title_buttons(l);
    for (i = 0; i < NSYN; ++i)
        set(&l->syn_row[i], 222, (ntsc ? 24 : 26) + i * 9, 92, 9);
    for (i = 0; i < RULE_KITSIZE; ++i) {
        if (ntsc)
            set(&l->kit_chip[i], 223 + (i % 2) * 47, 99 + (i / 2) * 12, 44, 11);
        else
            set(&l->kit_chip[i], 223 + (i % 2) * 47, 109 + (i / 2) * 15, 44, 13);
    }
}

unsigned char layout_inside(const struct rect *r, short x, short y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

short layout_node_at(const struct layout *l, short x, short y)
{
    short dx = x - l->grid_x, dy = y - l->grid_y;

    if (dx < 0 || dy < 0 || dx >= RULE_COLS * l->cell_w || dy >= RULE_ROWS * l->cell_h)
        return -1;
    return (dy / l->cell_h) * RULE_COLS + dx / l->cell_w;
}

void layout_node_rect(const struct layout *l, unsigned char node, struct rect *r)
{
    set(r, l->grid_x + (node % RULE_COLS) * l->cell_w, l->grid_y + (node / RULE_COLS) * l->cell_h, l->cell_w, l->cell_h);
}

signed char layout_syn_at(const struct layout *l, short x, short y)
{
    signed char i;
    for (i = 0; i < NSYN; ++i)
        if (layout_inside(&l->syn_row[i], x, y))
            return i;
    return -1;
}

signed char layout_kit_at(const struct layout *l, short x, short y)
{
    signed char i;
    for (i = 0; i < RULE_KITSIZE; ++i)
        if (layout_inside(&l->kit_chip[i], x, y))
            return i;
    return -1;
}

signed char layout_seat_at(const struct layout *l, short x, short y)
{
    signed char i;
    if (l->ntsc)
        return -1;
    for (i = 0; i < RULE_MAXPLAYERS; ++i)
        if (layout_inside(&l->seat[i], x, y))
            return i;
    return -1;
}

signed char layout_btn_in(const struct layout *l, short x, short y, unsigned char lo, unsigned char hi)
{
    unsigned char i;
    for (i = lo; i <= hi && i < UI_BTN_MAX; ++i)
        if (l->btn[i].w && layout_inside(&l->btn[i], x, y))
            return i;
    return -1;
}

signed char layout_btn_at(const struct layout *l, short x, short y)
{
    return layout_btn_in(l, x, y, 0, UI_BTN_MAX - 1);
}
