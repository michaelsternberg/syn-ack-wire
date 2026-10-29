/* The Adam's screen (text.h): the TMS9918A in Graphics II, 768 cells each with
 * its own pattern and colours, painted from txt_cells and txt_pal against a
 * copy of what the VDP holds, so only cells that changed are written: a
 * pattern's 8 bytes when the glyph changed, 8 colour bytes when its colours
 * did. The display paints the rows it marked as its outermost call returns
 * (display_text.c); a row is painted with the VDP held (vdp.h), so the NMI
 * waits at most one frame. A whole screen takes about a fifth of a second.
 *
 * The glyphs are the NES's (src/nes/font.txt, the NES CONFIG's Family BASIC
 * style), a pattern each in the colours of its palette, swapped for inverse.
 * The quarter blocks $10-$1F (low nibble: the top left, top right, bottom left
 * and bottom right modules lit) are white on black, a QR code's light and dark.
 *
 * The title's static: the logo's strokes are 4 codes, $0C-$0F. In each third
 * of the screen it covers, the first cell of each code keeps its own tile and
 * the others are pointed at it in the name table, so the NMI makes static of
 * the whole logo by writing 8 tiles (nmi.asm). */
#include <string.h>
#include "text.h"
#include "vdp.h"
#include "../nes/font.h"

#define NOISE       0x0C                /* the static's 4 codes */
#define QR          0x10                /* the quarter blocks' 16 */
#define COL_WHITE   0xF1                /* white on black: the static, the quarter blocks */
#define TITLE_ROWS  16                  /* the thirds the logo may be in */

unsigned char txt_cells[TXT_ROWS * TXT_COLS], txt_pal[TXT_ROWS * TXT_COLS];
unsigned char help_cells[TXT_ROWS * TXT_COLS];
unsigned char txt_dirty[TXT_ROWS];
unsigned char sat[4 * SPRITES], sat_dirty;

extern unsigned char noise[264], st_n;  /* nmi.asm */
extern unsigned st_adr[8];

static unsigned char sh_glyph[TXT_ROWS * TXT_COLS], sh_col[TXT_ROWS * TXT_COLS];
static unsigned char title_on, quarter[8];
static unsigned char r, c, g, v;
static unsigned i;

/* each palette: glyph colour << 4 | background (TMS9918 colours) */
static const unsigned char colours[4] = {
    0x31,                               /* PAL_TEXT: light green on black */
    0xFC,                               /* PAL_PANEL: white on dark green */
    0xBC,                               /* PAL_PANEL2: light yellow on dark green */
    0xFD,                               /* PAL_ASK: white on magenta */
};

/* the sprites' patterns */
static const unsigned char sprites[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,     /* SPR_LINE */
    0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     /* SPR_TL */
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF,     /* SPR_BR */
    0x00, 0x40, 0x60, 0x70, 0x60, 0x40, 0x00, 0x00,     /* SPR_ARROW */
};

static const unsigned char blank[8];

static const unsigned char *pattern(unsigned char t)
{
    if (t >= 0x20 && t < 0x7F)
        return font_text + (unsigned)(t - 0x20) * 8;
    if (t >= 0x01 && t < 0x01 + FONT_BOXES)
        return font_box + (unsigned)(t - 0x01) * 8;
    if (t >= QR && t < QR + 16) {
        v = ((t & 8) ? 0xF0 : 0) | ((t & 4) ? 0x0F : 0);
        quarter[0] = quarter[1] = quarter[2] = quarter[3] = v;
        v = ((t & 2) ? 0xF0 : 0) | ((t & 1) ? 0x0F : 0);
        quarter[4] = quarter[5] = quarter[6] = quarter[7] = v;
        return quarter;
    }
    return blank;
}

/* Row r of `cells`, in the palettes `pals` (0: all PAL_TEXT). */
static void row(const unsigned char *cells, const unsigned char *pals)
{
    unsigned char *sg = sh_glyph + ((unsigned)r << 5), *sc = sh_col + ((unsigned)r << 5);

    vdp_adr = (unsigned)r << 8;
    VDP_LOCK();
    for (c = 0; c < TXT_COLS; ++c, vdp_adr += 8) {
        g = cells[c];
        if ((g & 0x7F) >= NOISE && (g & 0x7F) < QR + 16)
            v = COL_WHITE;
        else {
            v = colours[pals ? pals[c] : PAL_TEXT];
            if (g & TXT_INV)
                v = v << 4 | v >> 4;
        }
        g &= 0x7F;
        if (g != sg[c]) {
            sg[c] = g;
            vdp_src = pattern(g);
            vdp_put8();
        }
        if (v != sc[c]) {
            sc[c] = v;
            vdp_val = v;
            vdp_adr += VR_COL;
            vdp_fill8();
            vdp_adr -= VR_COL;
        }
    }
    VDP_UNLOCK();
}

static void sprites_out(unsigned char n)
{
    VDP_LOCK();
    for (vdp_adr = VR_SAT, vdp_src = sat; n; --n, vdp_adr += 8, vdp_src += 8)
        vdp_put8();
    VDP_WADDR(vdp_adr);
    VDP_DATA = 0xD0;                    /* the end of the list */
    VDP_UNLOCK();
}

/* Graphics II, every cell a space in PAL_TEXT, the name table 0-255 in each
 * third; the display stays off until txt_show. */
void txt_init(void)
{
    unsigned lfsr = 0xACE1;

    VDP_LOCK();
    VDP_REG(1, 0xA0);                   /* 16K, the display off, the interrupt on */
    VDP_REG(0, 0x02);                   /* Graphics II */
    VDP_REG(2, 0x06);                   /* names 1800h */
    VDP_REG(3, 0xFF);                   /* colours 2000h */
    VDP_REG(4, 0x03);                   /* patterns 0000h */
    VDP_REG(5, 0x36);                   /* sprite attributes 1B00h */
    VDP_REG(6, 0x07);                   /* sprite patterns 3800h */
    VDP_REG(7, 0x01);                   /* the border black */
    VDP_WADDR(VR_PAT);                  /* (the display off: no wait between bytes) */
    for (i = 0; i < 0x1800; ++i)
        VDP_DATA = 0;
    for (i = 0; i < 768; ++i)
        VDP_DATA = (unsigned char)i;
    VDP_DATA = 0xD0;                    /* no sprites */
    VDP_WADDR(VR_COL);
    for (i = 0; i < 0x1800; ++i)
        VDP_DATA = colours[PAL_TEXT];
    VDP_WADDR(VR_SPAT);
    for (i = 0; i < sizeof sprites; ++i)
        VDP_DATA = sprites[i];
    VDP_UNLOCK();
    memset(sh_glyph, ' ', sizeof sh_glyph);
    memset(sh_col, colours[PAL_TEXT], sizeof sh_col);
    memset(txt_cells, ' ', sizeof txt_cells);
    for (i = 0; i < sizeof noise; ++i) {    /* the static's random bytes: a 16-bit Galois LFSR, */
        for (c = 0; c < 8; ++c)             /* 8 steps a byte (one a byte would be its neighbour shifted: stripes) */
            lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u);
        noise[i] = (unsigned char)lfsr;
    }
}

void txt_show(void)
{
    VDP_LOCK();
    VDP_REG(1, 0xE0);                   /* the display on */
    VDP_UNLOCK();
}

void txt_paint(void)
{
    if (help_up)
        return;
    for (r = 0; r < TXT_ROWS; ++r)
        if (txt_dirty[r]) {
            txt_dirty[r] = 0;
            row(txt_cells + ((unsigned)r << 5), txt_pal + ((unsigned)r << 5));
        }
    if (sat_dirty) {
        sat_dirty = 0;
        sprites_out(SPRITES * 4 / 8);
    }
}

void txt_paint_help(void)
{
    sprites_out(0);
    for (r = 0; r < TXT_ROWS; ++r)
        row(help_cells + ((unsigned)r << 5), 0);
}

/* On: the static's tiles found in what the title painted, the other stroke
 * cells pointed at them, and the NMI writing them. Off: the name table as
 * it was, and the static's tiles painted again with whatever is there next. */
void txt_title(unsigned char on)
{
    unsigned char home[4], t, k;

    noise_on = 0;
    if (title_on) {
        title_on = 0;
        VDP_LOCK();
        vdp_adr = VR_NAME;
        vdp_val = 0;
        vdp_n = 0;
        vdp_names();                    /* rows 0-7 */
        vdp_adr = VR_NAME + 256;
        vdp_names();                    /* 8-15 */
        VDP_UNLOCK();
        for (k = 0; k < st_n; ++k) {
            i = st_adr[k] >> 3;         /* the cell */
            sh_glyph[i] = 0xFF;
        }
    }
    st_n = 0;
    if (!on)
        return;
    title_on = 1;
    for (t = 0; t < TITLE_ROWS; t += 8) {
        memset(home, 0xFF, 4);
        for (r = t; r < t + 8; ++r)
            for (c = 0; c < TXT_COLS; ++c) {
                k = txt_cells[((unsigned)r << 5) + c] - NOISE;
                if (k >= 4)
                    continue;
                if (home[k] == 0xFF) {      /* this one's tile is the static's */
                    home[k] = (r & 7) << 5 | c;
                    st_adr[st_n++] = (unsigned)r << 8 | c << 3;
                } else {
                    VDP_LOCK();
                    VDP_WADDR(VR_NAME + ((unsigned)r << 5) + c);
                    VDP_DATA = home[k];
                    VDP_UNLOCK();
                }
            }
    }
    noise_on = st_n != 0;
}
