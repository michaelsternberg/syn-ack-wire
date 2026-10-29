/* What src/nes/display_text.c needs from the rest of the NES client, on a PC
 * (make pc-nes), besides input.c's cursor: the cells, the attribute table and the sprites, which
 * test/pc_harness.c's pc_dump saves (1280 bytes: 960 cells, 64 attributes,
 * 256 of OAM; tools/render_nes.py draws them); no PPU, no pad. */
#include <string.h>
#include "../src/nes/text.h"

unsigned char txt_cells[TXT_ROWS * TXT_COLS], help_cells[TXT_ROWS * TXT_COLS];
unsigned char txt_dirty[2 * TXT_ROWS], attr[128], attr_dirty[16], any_dirty, noise_on, noise[256];
unsigned char ppu_ctrl, ppu_mask, nes_tps = 60;
volatile unsigned char frames, ppu_direct;
unsigned char pc_oam[256];
unsigned char help_up;                  /* help.c's; the cursor is src/nes/input.c's, linked too */

void txt_init(void)
{
    memset(txt_cells, ' ', sizeof txt_cells);
    memset(pc_oam, 0xF8, sizeof pc_oam);
}

void txt_show(void) {}
