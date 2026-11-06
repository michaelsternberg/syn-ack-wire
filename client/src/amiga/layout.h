/* Where everything is on the Amiga screen, from the screen's height; and which widget a pixel belongs to.
 * Pure, no OS headers, so it is tested on the host. The numbers are the approved mockups' (tools/amiga_mock.py). */
#ifndef AMIGA_LAYOUT_H
#define AMIGA_LAYOUT_H

#include "../common/theme_tab.h"

struct rect { short x, y, w, h; };

/* the buttons of the dialogs and the title, found by layout_btn_at() */
enum {
    BTN_BUY, BTN_UNDO, BTN_NULLROUTE, BTN_YES, BTN_NO, BTN_OK, BTN_PLUS, BTN_MINUS,
    BTN_SEAT2, BTN_SEAT3, BTN_SEAT4, BTN_SEAT5, BTN_SEAT6,      /* the seat-count chips, 2 to 6 */
    BTN_JACK, BTN_HINTS, BTN_SOUND, BTN_HELP, BTN_QUIT,         /* the title's */
    UI_BTN_MAX
};

struct layout {
    short width, height;
    unsigned char ntsc;                 /* fewer than 256 lines */
    struct rect header, grid, syn_panel, kit_panel, log, prompt, clock;
    short cell_w, cell_h, grid_x, grid_y;           /* the top-left pixel of node 0 */
    struct rect syn_row[NSYN];          /* a syndicate's line in the side panel */
    struct rect kit_chip[RULE_KITSIZE];
    unsigned char seat_cards;           /* PAL: one card for each seat. NTSC: 2 (me, the seat to move) and the list */
    struct rect seat[RULE_MAXPLAYERS];
    struct rect others, scroll_up, scroll_down;     /* NTSC only: the other seats' list and its arrows */
    struct rect seats_area;             /* all of it: the cards or the table, and a click that flips between them */
    struct rect help_page, help_prev, help_exit, help_next;   /* the instruction page (it covers the screen), and the click zones of its footer */
    struct rect dlg, dlg_row[NSYN];     /* the buy window, and its syndicate rows */
    struct rect tbl_buy;                /* the table of every netrunner while buying: PAL the seats' area, NTSC taller */
    struct rect dlg_small, ask;         /* the hivejack and seat-count window (the buy window's top), and a Y/N question */
    struct rect btn[UI_BTN_MAX];        /* empty (w == 0) until the dialog or title that owns it fills it in */
};

/* PAL for 256 lines or more, NTSC below; other heights are clamped to the nearer of 200 and 256's layouts. */
void layout_make(struct layout *l, unsigned short height);

unsigned char layout_inside(const struct rect *r, short x, short y);        /* half-open */
short layout_node_at(const struct layout *l, short x, short y);             /* 0-107 (row * RULE_COLS + col), or -1 */
void layout_node_rect(const struct layout *l, unsigned char node, struct rect *r);
signed char layout_syn_at(const struct layout *l, short x, short y);        /* a side-panel row, 0-6, or -1 */
signed char layout_kit_at(const struct layout *l, short x, short y);        /* a kit chip, 0-5, or -1 */
signed char layout_seat_at(const struct layout *l, short x, short y);       /* PAL: a seat's card; NTSC: -1 */
signed char layout_btn_at(const struct layout *l, short x, short y);        /* a BTN_*, or -1 */
/* The same among lo..hi only: the title's buttons share their band with the NTSC buy window's, so each context
 * looks among its own. */
signed char layout_btn_in(const struct layout *l, short x, short y, unsigned char lo, unsigned char hi);

#endif
