/* src/coco/text.c's calls on a PC (make pc-coco), for src/coco/display_text.c:
 * no screen, only the cells, which test/pc_harness.c's pc_dump saves (960
 * bytes, a cell a byte, bit 7 inverse); tools/render_coco.py --cells draws them
 * as text.c would. */
#include <string.h>
#include "../src/coco/text.h"

unsigned char txt_cells[TXT_ROWS * TXT_COLS];
unsigned char *txt_src = txt_cells;
unsigned char helping;                  /* help.c's */

void help_flip(void) {}
void txt_init(void) { memset(txt_cells, ' ', sizeof txt_cells); }
void txt_show(void) {}
void txt_colours(unsigned char green_bg) { (void)green_bg; }
void txt_clear(void) {}
void txt_row(unsigned char row) { (void)row; }
void txt_all(void) {}
