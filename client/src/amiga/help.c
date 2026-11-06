#include "../platform.h"
#include "../common/game.h"
#include "gfx.h"
#include "help.h"
#include "help_tab.h"

#define SCALE 4                         /* a QR module's side in pixels: 37 modules with the border fit the 160 above the footer */

unsigned char help_pages(void)
{
    return HELP_PAGES;
}

/* one run of a row: reverse video is the cyan bar, and a lone syndicate letter in it takes its syndicate's colour */
static void run(short x, short y, const char *s, unsigned char n, unsigned char rev)
{
    unsigned char j, bg = PEN_BG0, pen = PEN_TEXT;
    if (!n)
        return;
    if (rev) {
        bg = PEN_CYAN;
        pen = PEN_BG0;
        if (n == 1)
            for (j = 0; j < NSYN; ++j)
                if (s[0] == syn_letter[j])
                    bg = PEN_SYN + j;
    }
    gfx_text(x, y, s, n, pen, bg);
}

static void draw_qr(const struct rect *pg)
{
    struct rect r;
    unsigned char x, y, size = (HELP_QR_N + 8) * SCALE;
    unsigned short i;
    r.x = (320 - size) / 2; r.y = pg->y + (160 - size) / 2; r.w = size; r.h = size;
    gfx_rect(&r, PEN_WHITE);
    r.w = r.h = SCALE;
    for (y = 0; y < HELP_QR_N; ++y)
        for (x = 0; x < HELP_QR_N; ++x) {
            i = y * HELP_QR_N + x;
            if (help_qr[i >> 3] & (0x80 >> (i & 7))) {
                r.x = (320 - size) / 2 + (x + 4) * SCALE;
                r.y = pg->y + (160 - size) / 2 + (y + 4) * SCALE;
                gfx_rect(&r, PEN_BG0);
            }
        }
}

void help_draw(unsigned char page)
{
    const struct rect *pg = &amiga_layout.help_page;
    const unsigned char *p;
    struct rect all;
    char b[40];
    unsigned char row, i, n, col, len, rev;

    if (page < 1 || page > HELP_PAGES)
        return;
    all.x = 0; all.y = 0; all.w = amiga_layout.width; all.h = amiga_layout.height;
    gfx_rect(&all, PEN_BG0);
    p = help_data + help_off[page - 1];
    for (row = 0; row < 24; ++row) {
        len = *p++;
        rev = n = col = 0;
        for (i = 0; i < len; ++i) {
            if (p[i] == 1) {
                run(8 * (col - n), pg->y + row * 8, b, n, rev);
                n = 0;
                rev ^= 1;
            } else if (n < sizeof b) {
                b[n++] = p[i];
                ++col;
            }
        }
        run(8 * (col - n), pg->y + row * 8, b, n, rev);
        p += len;
    }
    if (page == HELP_QR_PAGE)
        draw_qr(pg);
}
