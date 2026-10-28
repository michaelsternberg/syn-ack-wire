/* The CoCo 1/2's text screen: 40x24 cells of 6x8 pixels on PMODE 4 (256x192,
 * one bit a pixel), drawn from a copy of the cells (txt_cells) a row at a time.
 * The display writes the cells, then draws the rows it changed. */
#ifndef TEXT_H
#define TEXT_H

#define TXT_COLS    40
#define TXT_ROWS    24
#define TXT_INV     0x80                /* a cell's bit 7: inverse video */

/* row by row, 40 a row: ASCII $20-$5F or the box glyphs $60-$6A, | TXT_INV for inverse */
extern unsigned char txt_cells[TXT_ROWS * TXT_COLS];
extern unsigned char *txt_src;          /* the cells drawn: txt_cells, or another screen's (help) */

void txt_init(void);                    /* PMODE 4 at $0E00, cleared, green on black, not yet shown */
void txt_show(void);                    /* show it */
void txt_colours(unsigned char green_bg);   /* 1: black on green; redraws it all */
void txt_clear(void);                   /* every pixel the background */
void txt_row(unsigned char row);        /* draw one row from txt_src */
void txt_all(void);                     /* draw every row */

#endif
