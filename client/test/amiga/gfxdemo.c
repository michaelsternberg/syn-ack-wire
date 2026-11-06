/* Rig check for src/amiga/gfx.c: opens the screen, draws every primitive and the 32 pens, logs the free chip RAM
 * before and after, waits, closes. Output goes to stdout (the rig's SYNACK:log). */
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include "../../src/amiga/gfx.h"

int main(void)
{
    unsigned short w, h;
    unsigned long before = AvailMem(MEMF_CHIP);
    int i;
    struct rect r, a = { 4, 30, 60, 20 }, b = { 70, 30, 60, 20 }, c = { 136, 30, 60, 20 }, d = { 202, 30, 60, 20 };

    printf("chip before %lu\n", before);
    fflush(stdout);
    if (!gfx_open(&w, &h)) { printf("gfx_open failed\n"); return 20; }
    printf("screen %ux%u\n", w, h);
    for (i = 0; i < 32; ++i) {
        r.x = 4 + (i % 16) * 19; r.y = 4 + (i / 16) * 10; r.w = 18; r.h = 9;
        gfx_rect(&r, i);
    }
    gfx_bevel(&a, PEN_BG1, PEN_EDGEHI, PEN_EDGE);
    gfx_frame(&b, PEN_CYAN);
    gfx_hatch(&c, PEN_SYN, PEN_SYN_DIM);
    r.x = 202; r.y = 30; r.w = 60; r.h = 20; gfx_rect(&r, PEN_SYN + 3);
    gfx_dim(&d, PEN_BG0);
    gfx_text(4, 60, "TOPAZ 8 TEXT", 12, PEN_TEXT, PEN_BG0);
    gfx_text_right(316, 60, "RIGHT 123", 9, PEN_AMBER, PEN_BG0);
    gfx_text(4, h - 12, "BOTTOM ROW", 10, PEN_WHITE, PEN_BG1);
    printf("drawn\n");
    fflush(stdout);
    Delay(50 * 12);
    gfx_close();
    printf("chip after %lu\n", (unsigned long)AvailMem(MEMF_CHIP));
    fflush(stdout);
    return 0;
}
