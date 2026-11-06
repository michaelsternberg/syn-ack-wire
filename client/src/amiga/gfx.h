/* Drawing on the game's 5-plane screen: rectangles, bevels, Topaz 8 text. Every call paints its own piece, so the
 * display functions redraw what changed and nothing is kept in a frame buffer. Coordinates are screen pixels. */
#ifndef AMIGA_GFX_H
#define AMIGA_GFX_H

#include "layout.h"
#include "palette.h"

extern struct layout amiga_layout;      /* filled by gfx_open for the screen it got */

unsigned char gfx_open(unsigned short *width, unsigned short *height);   /* 0 on failure, with nothing left open */
void gfx_close(void);                   /* the window, then the screen; safe if never opened */

void gfx_rect(const struct rect *r, unsigned char pen);
void gfx_line(short x0, short y0, short x1, short y1, unsigned char pen);
void gfx_dot(short x, short y, unsigned char pen);
unsigned char gfx_peek(short x, short y);   /* the pen at a pixel */
void gfx_frame(const struct rect *r, unsigned char pen);                 /* a 1-pixel outline just inside r */
void gfx_bevel(const struct rect *r, unsigned char fill, unsigned char hi, unsigned char lo);
void gfx_text(short x, short y, const char *s, unsigned char n, unsigned char pen, unsigned char bg);   /* y: the glyph top */
void gfx_text_right(short right, short y, const char *s, unsigned char n, unsigned char pen, unsigned char bg);
void gfx_glyph(short x, short y, unsigned char ch, unsigned char scale, unsigned char pen);   /* a Topaz glyph, each pixel scale x scale */
void gfx_hatch(const struct rect *r, unsigned char pen_a, unsigned char pen_b);   /* the Black ICE armour */
unsigned char gfx_save(const struct rect *r);   /* copy that part of the screen aside (chip RAM); 0 if there is no memory for it */
void gfx_discard(void);                 /* free the saved part without putting it back (the screen was repainted meanwhile) */
void gfx_restore(void);                 /* put the saved part back and free it */
void gfx_cycle(unsigned char first, unsigned char n, unsigned char step);   /* rotate n palette entries from first (the title's neon) */
void gfx_art(const unsigned char *packed, unsigned short len, const unsigned short *palette);   /* the title picture, over the screen, in its palette */
void gfx_cycle_end(void);               /* the palette as it was loaded */
void gfx_dim(const struct rect *r, unsigned char pen);                   /* a checkerboard of pen over what is there */

#endif
