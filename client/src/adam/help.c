/* The instruction pages for the Adam. They are in the program, packed by
 * tools/help_nes.py from help/adam/screen/HELP*.TXT (help_pages.h, made at
 * build time): a page is drawn into help_cells and painted over the screen,
 * without the sprites (the controller's cursor). While the pages are up the
 * game goes on writing its own cells underneath, unpainted (text.c), and
 * closing paints them all again, as on the CoCo. The last page shown opens
 * again next time. */
#include <string.h>
#include "../platform.h"
#include "../display.h"
#include "text.h"
#include "help_pages.h"                 /* help_data[], help_page[], HELP_PAGES */

extern unsigned char field;             /* display_text.c: the playfield is up */

unsigned char help_up;
static unsigned char page, r;

static void draw(void)
{
    const unsigned char *s = help_data + help_page[page];
    unsigned char *d, c;

    memset(help_cells, ' ', TXT_ROWS * TXT_COLS);
    for (r = HELP_TOP; r < HELP_TOP + HELP_ROWS; ++r) {
        d = help_cells + ((unsigned)r << 5) + HELP_LEFT;
        while ((c = *s++) != 0)
            if (c < 0x10)
                d += c;                 /* that many spaces */
            else
                *d++ = c;
    }
    txt_paint_help();
}

void help_open(void)
{
    txt_title(0);                       /* the title's static off: its tiles are the page's now */
    help_up = 1;
    draw();
}

unsigned char __fastcall__ help_key(unsigned char k)
{
    if (k == '>' && page + 1 < HELP_PAGES) {
        ++page;
        draw();
    } else if (k == '<' && page) {
        --page;
        draw();
    } else if (k == K_ESC || k == K_HELP || k == K_RETURN) {
        help_up = 0;
        memset(txt_dirty, 1, TXT_ROWS);
        sat_dirty = 1;
        txt_paint();
        if (!field)
            txt_title(1);
        return 0;
    }
    return 1;
}
