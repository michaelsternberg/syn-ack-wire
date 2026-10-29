/* The NES's screen: two name tables of 32x30 tiles, the game's ($2000) and the
 * instructions' ($2400), kept in WRAM as txt_cells and help_cells. The
 * program writes cells there; the NMI copies what changed to the PPU in the
 * vertical blank (ppu.s). Rows 0 and 29 are under a TV's overscan: the screens
 * use rows 1-28 and columns 1-30.
 *
 * A cell is a background tile: ASCII $20-$7E, the box tiles $01-$0B, the
 * static's $0C-$0F, the quarter blocks $10-$1F; | TXT_INV for a glyph's
 * inverse. A glyph is colour 3 on colour 2 of its palette (inverse: 2 on 3),
 * so each palette gives its text its own background: the 2x2-tile blocks of
 * the attribute table pick them. */
#ifndef TEXT_H
#define TEXT_H

#define TXT_COLS    32
#define TXT_ROWS    30
#define TXT_INV     0x80

/* the background palettes */
#define PAL_TEXT    0                   /* green on black; colour 1 white (the static, the QR code) */
#define PAL_PANEL   1                   /* pale green on dark green: the Grid and the seats */
#define PAL_PANEL2  2                   /* yellow on dark green: cash, every other syndicate's crypto */
#define PAL_ASK     3                   /* white on purple: this seat has a decision to make */

/* sprite tiles */
#define SPR_LINE    1                   /* an underline */
#define SPR_TL      2                   /* a box's top and left edges */
#define SPR_BR      3                   /* its bottom and right edges */
#define SPR_ARROW   4                   /* pointing right */

extern unsigned char txt_cells[TXT_ROWS * TXT_COLS], help_cells[TXT_ROWS * TXT_COLS];
extern unsigned char txt_dirty[2 * TXT_ROWS];   /* a row written: 0-29 txt_cells, 30-59 help_cells */
extern unsigned char attr[128], attr_dirty[16]; /* the two attribute tables, 8 rows of 8 each */
extern unsigned char any_dirty;                 /* set after marking a row */
extern unsigned char noise_on;                  /* the title's static runs */
extern unsigned char noise[256];
extern unsigned char ppu_ctrl, ppu_mask, nes_tps;
extern volatile unsigned char frames, ppu_direct;
#ifdef __CC65__
#pragma zpsym ("frames")
#pragma zpsym ("ppu_direct")
#define OAM         ((unsigned char *)0x0200)
#else
extern unsigned char pc_oam[256];       /* make pc-nes: test/nes_text_pc.c */
#define OAM         pc_oam
#endif

void txt_init(void);                    /* tiles, palettes, both name tables blank; the display off */
void txt_show(void);                    /* txt_cells and the game's attributes, then the display on */

#endif
