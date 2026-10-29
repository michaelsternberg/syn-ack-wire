/* The Adam's screen: the TMS9918A in Graphics II, 32x24 cells, each with a
 * pattern and colours of its own (the name table holds 0-255 in each third of
 * the screen), so any cell can show any glyph in any colours. The program
 * writes cells into txt_cells, their palettes into txt_pal, and marks rows in
 * txt_dirty; txt_paint() writes to the VDP what changed, cell by cell, against
 * a copy of what it holds (text.c).
 *
 * A cell is as on the NES (src/nes/text.h), from the same font: ASCII
 * $20-$7E, the box glyphs $01-$0B, the static's $0C-$0F, the quarter blocks
 * $10-$1F; | TXT_INV for a glyph's inverse, its palette's two colours swapped. */
#ifndef TEXT_H
#define TEXT_H

#define TXT_COLS    32
#define TXT_ROWS    24
#define TXT_INV     0x80

/* the palettes: a glyph's colour on its background */
#define PAL_TEXT    0                   /* light green on black */
#define PAL_PANEL   1                   /* white on dark green: the Grid and the seats */
#define PAL_PANEL2  2                   /* yellow on dark green: cash, every other syndicate's crypto */
#define PAL_ASK     3                   /* white on magenta: this seat has a decision to make */

/* sprite patterns */
#define SPR_LINE    0                   /* an underline */
#define SPR_TL      1                   /* a box's top and left edges */
#define SPR_BR      2                   /* its bottom and right edges */
#define SPR_ARROW   3                   /* pointing right */
#define SPRITES     6                   /* the cursor's */

extern unsigned char txt_cells[TXT_ROWS * TXT_COLS], txt_pal[TXT_ROWS * TXT_COLS];
extern unsigned char help_cells[TXT_ROWS * TXT_COLS];
extern unsigned char txt_dirty[TXT_ROWS];   /* a row of txt_cells written */
extern unsigned char sat[4 * SPRITES];      /* the cursor's sprites: y, x, pattern, colour */
extern unsigned char sat_dirty;
extern unsigned char noise_on;              /* the title's static runs (the NMI) */
extern unsigned char help_up;               /* help.c: the instructions are up */
extern volatile unsigned char frames;       /* nmi.asm: the frames counted */

void txt_init(void);                    /* Graphics II, the sprites' patterns, a blank screen; the display off */
void txt_show(void);                    /* the display on */
void txt_paint(void);                   /* the marked rows and the sprites, unless the instructions are up */
void txt_paint_help(void);              /* help_cells, all of it, without the sprites */
void txt_title(unsigned char on);       /* the logo's strokes onto the static's tiles, or back */

#endif
