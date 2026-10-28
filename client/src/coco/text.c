/* The CoCo 1/2's text screen (text.h): 40x24 cells of 6x8 pixels on PMODE 4.
 *
 * Cell c of a row starts at pixel 8 + 6c, so the 40 columns are centred with 8
 * pixels either side, and every cell starts on an even pixel (font.txt keeps
 * vertical strokes on even columns, so NTSC artifacting tints them alike).
 * Four cells make 3 bytes, so a row is drawn 4 cells at a time, each of its 8
 * pixel lines written whole: no reading back, no masks. A glyph is a byte a
 * line, its 6 pixels in bits 7-2, turned over by its cell's flip:
 *
 *     byte 0 = g0 | g1 >> 6     byte 1 = g1 << 2 | g2 >> 4     byte 2 = g2 << 4 | g3 >> 2
 *
 * Inverse cells take a margin from their neighbours. A glyph fills columns 0-4
 * and rows 0-6, so the gap is on its right and below, and an inverse letter's
 * left and top strokes would run into the dark around it. So the pixel column
 * left of a run of inverse cells, and for a short run (a hive's letter) the
 * line above it too, take the inverse background. Longer runs (the bars) go
 * without the line above, which would sit on the text of the row above.
 *
 * The margins are in the flips, so every pixel is written once, as it ends up,
 * and a row drawn again over itself doesn't flicker: a cell left of an inverse
 * one flips its gap column (bit 2) to the inverse background, and on line 7 (its
 * gap row) a cell over a short run is all inverse background, the corner too. */
#include <coco.h>
#include "text.h"
#include "font.h"

#define SCREEN      ((unsigned char *)0x0E00)
#define SHORT_RUN   4                   /* inverse runs shorter than this get the line above */
#define GAP_COL     0x04                /* a glyph byte's sixth pixel */

unsigned char txt_cells[TXT_ROWS * TXT_COLS];
unsigned char *txt_src = txt_cells;
static unsigned char flip;              /* $FC: black on green */

/* each cell's flip for lines 0-6, then for line 7 (l_flips + 40) */
static unsigned char l_flips[2 * TXT_COLS];
/* for lines(): its arguments, then 4 cells' glyph address and flips, and temporaries */
static unsigned char *l_src, *l_scr, *l_fp;
static unsigned char l_first, l_end, l_groups, l_n, l_t0, l_t1, l_t2;
static unsigned char l_cells[16];
static unsigned char l_byte0, l_byte0_7;  /* pixels 0-7: background, or the column left of an inverse cell 0 */

/* 1 if the row has an inverse cell, quickly: most rows have none. */
static unsigned char any_inverse(const unsigned char *c)
{
    unsigned char any;

    asm {
        ldx     :c
        ldb     #40
        clra
@scan   tst     ,x+
        bmi     @found
        decb
        bne     @scan
        bra     @done
@found  inca
@done   sta     :any
    }
    return any;
}

/* The row's short inverse runs: 1 if it has any. If mark is set, each puts the
 * line above it into the line-7 flips (l_flips + 40): the run's cells all
 * inverse background, and the gap column of the cell before. */
static unsigned char short_runs(unsigned char row, unsigned char mark)
{
    const unsigned char *c = txt_src + (unsigned)row * TXT_COLS;
    unsigned char col = 0, start, k, found = 0;
    unsigned char inv = (unsigned char)(0xFC ^ flip);

    if (!any_inverse(c))
        return 0;
    while (col < TXT_COLS) {
        if (!(c[col] & TXT_INV)) {
            ++col;
            continue;
        }
        start = col;
        while (col < TXT_COLS && (c[col] & TXT_INV))
            ++col;
        if (col - start >= SHORT_RUN)
            continue;
        found = 1;
        if (!mark)
            continue;
        for (k = start; k < col; ++k)
            l_flips[TXT_COLS + k] = inv;
        if (start)
            l_flips[TXT_COLS + start - 1] = (unsigned char)((l_flips[TXT_COLS + start - 1] & ~GAP_COL) | (inv & GAP_COL));
        else
            l_byte0_7 = flip ? 0xFE : 0x01;
    }
    return found;
}

/* Each cell's flips for drawing the row: from its own cells (inverse, and the
 * gap column left of an inverse cell) and the next row's short runs (line 7). */
static void flips(unsigned char row)
{
    const unsigned char *c = txt_src + (unsigned)row * TXT_COLS;
    unsigned char k, f;

    l_byte0 = flip ? 0xFF : 0;
    if (any_inverse(c)) {
        for (k = 0; k < TXT_COLS; ++k) {
            f = c[k] & TXT_INV ? 0xFC : 0;
            if (!f && k + 1 < TXT_COLS && (c[k + 1] & TXT_INV))
                f = GAP_COL;
            l_flips[k] = (unsigned char)(f ^ flip);
        }
        if (c[0] & TXT_INV)
            l_byte0 ^= 0x01;
    } else
        memset(l_flips, flip, TXT_COLS);
    memcpy(l_flips + TXT_COLS, l_flips, TXT_COLS);
    l_byte0_7 = l_byte0;
    if (row + 1 < TXT_ROWS)
        short_runs((unsigned char)(row + 1), 1);
}

/* Lines first to last (0-7) of a row, from its cells and l_flips: 10 groups of
 * 4 cells, then pixels 0-7 and 248-255, all margin. */
static void lines(unsigned char row, unsigned char first, unsigned char last)
{
    l_src = txt_src + (unsigned)row * TXT_COLS;
    l_scr = SCREEN + ((unsigned)row << 8) + ((unsigned)first << 5) + 1;
    l_fp = l_flips;
    l_first = first;
    l_end = (unsigned char)(last + 1);
    asm {
        pshs    y,u
        lda     #10
        sta     _l_groups
@group  ldx     _l_src
        ldy     #_l_cells
        ldu     _l_fp
        lda     #4
        sta     _l_n
@cell   lda     ,u                      ; its flip for lines 0-6
        sta     2,y
        lda     40,u                    ; and for line 7
        sta     3,y
        leau    1,u
        ldb     ,x+                     ; the cell: bit 7 inverse
        lslb                            ; code * 2
        subb    #$40                    ; (code - $20) * 2
        clra
        lslb
        rola
        lslb
        rola                            ; (code - $20) * 8
        addd    #_font
        std     ,y                      ; its glyph
        leay    4,y
        dec     _l_n
        bne     @cell
        stx     _l_src
        stu     _l_fp
        ldu     _l_scr
        ldb     _l_first                ; B: the line, an offset into each glyph
@line   cmpb    #7                      ; line 7: the line-7 flips
        bne     @draw
        lda     _l_cells+3
        sta     _l_cells+2
        lda     _l_cells+7
        sta     _l_cells+6
        lda     _l_cells+11
        sta     _l_cells+10
        lda     _l_cells+15
        sta     _l_cells+14
@draw   ldx     _l_cells
        lda     b,x
        eora    _l_cells+2
        sta     _l_t0                   ; g0
        ldx     _l_cells+4
        lda     b,x
        eora    _l_cells+6
        sta     _l_t1                   ; g1
        rola
        rola
        rola
        anda    #3                      ; g1 >> 6
        ora     _l_t0
        sta     ,u
        lda     _l_t1
        lsla
        lsla
        sta     _l_t1                   ; g1 << 2
        ldx     _l_cells+8
        lda     b,x
        eora    _l_cells+10
        sta     _l_t2                   ; g2
        lsra
        lsra
        lsra
        lsra
        ora     _l_t1
        sta     1,u
        lda     _l_t2
        lsla
        lsla
        lsla
        lsla
        sta     _l_t2                   ; g2 << 4
        ldx     _l_cells+12
        lda     b,x
        eora    _l_cells+14
        lsra
        lsra                            ; g3 >> 2
        ora     _l_t2
        sta     2,u
        leau    32,u
        incb
        cmpb    _l_end
        lbne    @line
        ldu     _l_scr
        leau    3,u
        stu     _l_scr
        dec     _l_groups
        lbne    @group
        puls    y,u
    }
    for (l_scr = SCREEN + ((unsigned)row << 8) + ((unsigned)first << 5); first <= last; ++first, l_scr += 32) {
        l_scr[0] = first == 7 ? l_byte0_7 : l_byte0;
        l_scr[31] = flip ? 0xFF : 0;
    }
}

/* A row: its line 7 holds the margin over the next row's short runs, and the
 * row before's line 7 the margin over its own, which is drawn again (if before
 * is set) when the row has short runs or had them (tops[]). */
static unsigned char tops[TXT_ROWS];

static void draw(unsigned char row, unsigned char before)
{
    unsigned char has_short = row ? short_runs(row, 0) : 0;

    flips(row);
    lines(row, 0, 7);
    if (before && (has_short || tops[row])) {
        flips((unsigned char)(row - 1));
        lines((unsigned char)(row - 1), 7, 7);
    }
    tops[row] = has_short;
}

void txt_row(unsigned char row)
{
    draw(row, 1);
}

/* Every row, over whatever is there: each byte of the screen is written. */
void txt_all(void)
{
    unsigned char r;

    for (r = 0; r < TXT_ROWS; ++r)
        draw(r, 0);
}

void txt_clear(void)
{
    pcls(flip ? 0xFF : 0x00);
}

void txt_colours(unsigned char green_bg)
{
    flip = green_bg ? 0xFC : 0;
    txt_all();
}

/* The cells blank and the PMODE 4 screen black, but not yet shown: the text
 * screen BASIC leaves showing ($0400) goes black meanwhile. */
void txt_init(void)
{
    memset(txt_cells, ' ', sizeof txt_cells);
    memset((void *)0x0400, 0x80, 512);  /* a dark graphics block a cell */
    pmode(4, SCREEN);
    pcls(0);                            /* the 24 rows of spaces, green on black */
}

void txt_show(void)
{
    screen(1, 0);                       /* colour set 0: black and green */
}
