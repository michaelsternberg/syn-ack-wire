/* The NES's screen at start-up: the tiles in CHR-RAM, the palettes, both name
 * tables blank, all written straight to the PPU while the display is off
 * (ppu_direct keeps the NMI away from it); then, for the first screen,
 * txt_show writes it whole and turns the display on. From then on the NMI
 * draws (ppu.s).
 *
 * The background tiles come from src/nes/font.txt (font.h, tools/font_nes.py):
 * each glyph with plane 1 all set, so its pixels are colour 3 on colour 2, and
 * at code | $80 the other way round, for inverse. The quarter blocks $10-$1F
 * (low nibble: the top left, top right, bottom left and bottom right modules
 * lit) are colour 1 on colour 0: a QR code's light and dark. */
#include "text.h"
#include "font.h"

#define PPUSTATUS   (*(volatile unsigned char *)0x2002)
#define PPUADDR     (*(volatile unsigned char *)0x2006)
#define PPUDATA     (*(volatile unsigned char *)0x2007)

static const unsigned char palettes[32] = {
    0x0F, 0x30, 0x0F, 0x2A,             /* PAL_TEXT */
    0x0F, 0x30, 0x0A, 0x3A,             /* PAL_PANEL */
    0x0F, 0x30, 0x0A, 0x38,             /* PAL_PANEL2 */
    0x0F, 0x30, 0x04, 0x30,             /* PAL_ASK */
    0x0F, 0x38, 0x28, 0x30,             /* the sprites: yellow */
    0x0F, 0x38, 0x28, 0x30,
    0x0F, 0x38, 0x28, 0x30,
    0x0F, 0x38, 0x28, 0x30,
};

/* sprite tiles 1-4, plane 0 (colour 1) */
static const unsigned char sprites[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,     /* SPR_LINE */
    0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     /* SPR_TL */
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF,     /* SPR_BR */
    0x00, 0x40, 0x60, 0x70, 0x60, 0x40, 0x00, 0x00,     /* SPR_ARROW */
};

static unsigned char i, n, t, flip;

static void addr(unsigned a)
{
    (void)PPUSTATUS;
    PPUADDR = (unsigned char)(a >> 8);
    PPUADDR = (unsigned char)a;
}

static void fill(unsigned char v, unsigned char k)
{
    do
        PPUDATA = v;
    while (--k);
}

/* A tile: plane 0 from `bits` (or blank) turned over by `flip`, then plane 1. */
static void tile(const unsigned char *bits, unsigned char plane1)
{
    for (i = 0; i < 8; ++i)
        PPUDATA = (bits ? bits[i] : 0) ^ flip;
    fill(plane1, 8);
}

void txt_init(void)
{
    unsigned lfsr = 0xACE1;

    ppu_direct = 1;
    addr(0x0000);
    for (flip = 0;; flip = 0xFF) {
        for (t = 0; t < 0x80; ++t) {
            if (t >= 0x20 && t < 0x7F)
                tile(font_text + (unsigned)(t - 0x20) * 8, 0xFF);
            else if (t >= 0x01 && t < 0x01 + FONT_BOXES)
                tile(font_box + (unsigned)(t - 0x01) * 8, 0xFF);
            else if (t >= 0x10 && t < 0x20) {
                n = ((t & 8) ? 0xF0 : 0) | ((t & 4) ? 0x0F : 0);
                fill(n, 4);
                n = ((t & 2) ? 0xF0 : 0) | ((t & 1) ? 0x0F : 0);
                fill(n, 4);
                fill(0, 8);
            } else {
                fill(0, 16);
            }
        }
        if (flip)
            break;
    }
    addr(0x1010);                       /* sprite tiles 1-4: plane 0, then plane 1 clear */
    for (t = 0; t < 32; t += 8) {
        for (i = 0; i < 8; ++i)
            PPUDATA = sprites[t + i];
        fill(0, 8);
    }
    addr(0x2000);                       /* both name tables: spaces, attributes 0 */
    for (t = 0; t < 2; ++t) {
        for (n = 0; n < 4; ++n)
            fill(' ', n < 3 ? 0 : 192); /* 3 x 256 + 192 = 960 */
        fill(0, 64);
    }
    addr(0x3F00);
    for (i = 0; i < 32; ++i)
        PPUDATA = palettes[i];
    for (i = 0;; ++i) {                 /* the static's random bytes: a 16-bit Galois LFSR, */
        for (n = 0; n < 8; ++n)         /* 8 steps a byte (one a byte would be its neighbour shifted: stripes) */
            lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u);
        noise[i] = (unsigned char)lfsr;
        if (i == 255)
            break;
    }
}

void txt_show(void)
{
    unsigned k;

    ppu_direct = 1;
    addr(0x2000);
    for (k = 0; k < TXT_ROWS * TXT_COLS; ++k)
        PPUDATA = txt_cells[k];
    for (i = 0; i < 64; ++i)
        PPUDATA = attr[i];
    for (i = 0; i < TXT_ROWS; ++i)
        txt_dirty[i] = 0;
    for (i = 0; i < 8; ++i)
        attr_dirty[i] = 0;
    ppu_mask = 0x1E;                    /* background and sprites, the left 8 pixels too */
    ppu_direct = 0;
}
