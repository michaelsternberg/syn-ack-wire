/* The instruction pages for the NES. They are in the ROM, packed by
 * tools/help_nes.py from help/nes/screen/HELP*.TXT (help_pages.h, made at
 * build time): a page is drawn into help_cells, the second name table's copy,
 * and the NMI sends it to the PPU like any other rows (ppu.s). Opening them
 * waits until the whole page is there, then shows that name table instead of
 * the game's, without the sprites (the pad's cursor). The game goes on drawing
 * its own name table underneath, so closing simply shows it again, as on the
 * Atari and the Apple. The last page shown opens again next time; a turn of
 * the page shows as it is drawn, in an eighth of a second. */
#include <string.h>
#include "../platform.h"
#include "../display.h"
#include "text.h"
#include "help_pages.h"                 /* help_data[], help_page[], HELP_PAGES */

#define CTRL_HELP   0x01                /* PPUCTRL: the name table at $2400 */
#define MASK_HELP   0x0A                /* PPUMASK: the background only */
#define MASK_GAME   0x1E

extern unsigned char field;             /* display_text.c: the playfield is up */

unsigned char help_up;
static unsigned char page, r;

static void draw(void)
{
    const unsigned char *s = help_data + help_page[page];
    unsigned char *d, c;

    memset(help_cells, ' ', TXT_ROWS * TXT_COLS);
    for (r = 1; r <= HELP_ROWS; ++r) {
        d = help_cells + ((unsigned)r << 5) + 1;
        while ((c = *s++) != 0)
            if (c < 0x10)
                d += c;                 /* that many spaces */
            else
                *d++ = c;
    }
    for (r = TXT_ROWS; r < 2 * TXT_ROWS; ++r)
        txt_dirty[r] = 1;
    any_dirty = 1;
}

void help_open(void)
{
    unsigned char t;

    help_up = 1;
    noise_on = 0;
    draw();
    for (r = TXT_ROWS; r < 2 * TXT_ROWS; ++r)  /* every row queued... */
        while (txt_dirty[r])
            ;
    for (t = frames; (unsigned char)(frames - t) < 2;)    /* ...and copied */
        ;
    ppu_ctrl |= CTRL_HELP;
    ppu_mask = MASK_HELP;
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
        ppu_ctrl &= ~CTRL_HELP;
        ppu_mask = MASK_GAME;
        help_up = 0;
        noise_on = !field;
        return 0;
    }
    return 1;
}
