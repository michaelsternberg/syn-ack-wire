/* The autoplay build's screen dumps on linux (make linux-test): test/autoplay.c calls pc_dump() at each
 * new prompt (and every DUMP_TICKS, if not 0). With DUMPDIR set, each dump is the whole cell array in
 * DUMPDIR/scrNNN.bin: 24 x 80 records of nl_cell, little-endian { uint32 ch; uint8 fg, bg, attr; } padded to 8 bytes
 * (tools/linux_render.py reads them). */
#include <stdio.h>
#include <stdlib.h>
#include "../src/linux/screen.h"

void pc_dump(void)
{
    static int n;
    char path[512];
    const char *dir = getenv("DUMPDIR");
    FILE *f;
    if (!dir || n > 999)
        return;
    snprintf(path, sizeof path, "%s/scr%03d.bin", dir, n++);
    if ((f = fopen(path, "wb")) != NULL) {
        fwrite(nl_cells, sizeof(nl_cell), NL_ROWS * NL_COLS, f);
        fclose(f);
    }
}
