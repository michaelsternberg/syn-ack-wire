/* What src/adam/display_text.c needs from the rest of the Adam client, on a PC
 * (make pc-adam), besides input.c's cursor: the cells, their palettes and the
 * sprites, which test/pc_harness.c's pc_dump saves (1560 bytes: 768 cells, 768
 * palettes, 24 of sprite attributes; tools/render_adam.py draws them); no VDP,
 * no keyboard, no controller. */
#include <string.h>
#include "../src/adam/text.h"

unsigned char txt_cells[TXT_ROWS * TXT_COLS], txt_pal[TXT_ROWS * TXT_COLS];
unsigned char help_cells[TXT_ROWS * TXT_COLS], txt_dirty[TXT_ROWS], sat[4 * SPRITES], sat_dirty;
unsigned char noise_on;
volatile unsigned char frames;
unsigned char help_up;                  /* help.c's; the cursor is src/adam/input.c's, linked too */

void txt_init(void)
{
    memset(txt_cells, ' ', sizeof txt_cells);
}

void txt_show(void) {}
void txt_paint(void) {}
void txt_paint_help(void) {}
void txt_title(unsigned char on) { noise_on = on; }
