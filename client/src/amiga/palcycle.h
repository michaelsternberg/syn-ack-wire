/* Rotating a run of palette entries: pure, so tested on the host. */
#ifndef AMIGA_PALCYCLE_H
#define AMIGA_PALCYCLE_H

void palcycle(unsigned short *rgb, unsigned char n, unsigned char step);   /* entry i moves to (i + step) mod n */

#endif
