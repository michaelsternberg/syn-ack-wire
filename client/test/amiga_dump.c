/* The autoplay build's screen dump (test/autoplay.c's DUMP, every DUMP_TICKS and at each new prompt), for the manual's
 * captures: tools/capture.sh with PLATFORM=amiga builds it with DUMP_TICKS, and the rig's shared folder holds the
 * dumps, SYNACK:d/scrNNN.bin. tools/render_amiga.py draws one. Built with DUMP_TICKS=0 (amiga-test) it is never
 * called. A dump, big-endian as the 68000 writes it:
 *   "SAWD", rows (2 bytes), bytes per row (2), depth (1), 0 (1)
 *   32 colours, 0x0RGB (2 bytes each), from the screen's colour map as it is now (the title's are turning)
 *   the prompt strip's text and the title's status, 48 bytes each, 0-padded
 *   each bitplane, rows x bytes per row */
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <intuition/screens.h>
#include <string.h>
#include "../src/common/game.h"
#include "../src/amiga/gfx.h"

extern char amiga_strip_text[];         /* src/amiga/display_gfx.c */

static unsigned short count;

static void put_text(BPTR f, const char *s, unsigned char n)
{
    char b[48];
    memset(b, 0, sizeof b);
    memcpy(b, s, n < sizeof b ? n : sizeof b);
    Write(f, b, sizeof b);
}

void pc_dump(void)
{
    struct Screen *s = gfx_screen();
    struct BitMap *bm;
    char name[20] = "SYNACK:d/scr000.bin";
    unsigned char head[10], i;
    UWORD pal[32];
    BPTR f;

    if (!s || count > 999)
        return;
    bm = s->RastPort.BitMap;
    name[12] = '0' + count / 100;
    name[13] = '0' + count / 10 % 10;
    name[14] = '0' + count % 10;
    if (!(f = Open((STRPTR)name, MODE_NEWFILE)))
        return;
    ++count;
    memcpy(head, "SAWD", 4);
    head[4] = bm->Rows >> 8;
    head[5] = bm->Rows;
    head[6] = bm->BytesPerRow >> 8;
    head[7] = bm->BytesPerRow;
    head[8] = bm->Depth;
    head[9] = 0;
    Write(f, head, sizeof head);
    for (i = 0; i < 32; ++i)
        pal[i] = (UWORD)GetRGB4(s->ViewPort.ColorMap, i);
    Write(f, pal, sizeof pal);
    put_text(f, amiga_strip_text, strlen(amiga_strip_text));
    put_text(f, (const char *)status_text, status_len ? status_len - 1 : 0);
    for (i = 0; i < bm->Depth; ++i)
        Write(f, bm->Planes[i], (LONG)bm->Rows * bm->BytesPerRow);
    Close(f);
}
