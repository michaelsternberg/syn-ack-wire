/* How fast the CPU runs with the game's screen open: counts loop passes in 10 seconds (build with -DGFX_DEPTH=n). */
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include "../../src/amiga/gfx.h"

int main(void)
{
    unsigned short w, h;
    struct DateStamp a, b;
    volatile unsigned long n = 0, i;
    unsigned long start, now;

    if (!gfx_open(&w, &h)) { printf("gfx_open failed\n"); return 20; }
    DateStamp(&a);
    start = a.ds_Minute * 3000UL + a.ds_Tick;
    do {
        for (i = 0; i < 2000; ++i)
            n += i;
        DateStamp(&b);
        now = b.ds_Minute * 3000UL + b.ds_Tick;
        if (now < start)
            now += 1440 * 3000UL;
    } while (now - start < 500);            /* 10 s of 1/50 */
    gfx_close();
    printf("depth %d: %lu\n", GFX_DEPTH, n);
    fflush(stdout);
    return 0;
}
