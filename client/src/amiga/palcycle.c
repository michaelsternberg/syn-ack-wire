#include "palcycle.h"

void palcycle(unsigned short *rgb, unsigned char n, unsigned char step)
{
    unsigned char i, k;
    unsigned short t;

    if (n < 2)
        return;
    step %= n;
    for (k = 0; k < step; ++k) {        /* one place at a time: n is small, and no scratch array */
        t = rgb[n - 1];
        for (i = n - 1; i > 0; --i)
            rgb[i] = rgb[i - 1];
        rgb[0] = t;
    }
}
