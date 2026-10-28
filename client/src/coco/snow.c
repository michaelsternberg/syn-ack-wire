/* The title's static, like a TV with no signal: the logo's letters (the Atari's
 * title art in box glyphs) filled with random pixels, written straight to the
 * PMODE 4 screen, as the Atari's snow.s fills them with its players.
 *
 * title_snow.h (tools/title_art.py m) says which pixels of each line take it:
 * the letters' strokes, out to their outline but 2 pixels clear of it, so the
 * lines stand out of the static. Each line has a record of tokens: so many bytes
 * masked (pixels of the outline and the gap kept as they are), so many skipped,
 * so many all static. The random bytes come from a table with 30% of their bits
 * set, read from a fresh start (an LFSR) on every line.
 *
 * Each call does every other line, the even ones then the odd ones: about
 * 25,000 cycles, under 2 frames, so keys and the network don't wait long. */
#include "text.h"
#include "title_snow.h"

#define SCREEN  0x0E00                  /* text.c's */

static unsigned char seed = 1;          /* the LFSR: never 0 */
static unsigned char line;              /* the line drawing, from SNOW_TOP */
static unsigned char odd;               /* this call's lines: 0 even, 1 odd */
static unsigned char inv;               /* $FF: black on green, the static dark */

/* In the loops, X walks the random table, Y the record, U the screen and B
 * counts a token's bytes. The colours go into their two eora #0 (the code is in
 * RAM): an immediate is 3 cycles a byte quicker than reading inv. */
void snow(unsigned char green_bg)
{
    inv = green_bg ? 0xFF : 0;
    odd ^= 1;
    line = odd;
    asm {
        pshs    y,u
        lda     _inv
        sta     @mflip+1
        sta     @aflip+1
@line   ldb     _line
        clra
        addd    #SNOW_TOP
        lslb
        rola
        lslb
        rola
        lslb
        rola
        lslb
        rola
        lslb
        rola                            ; 32 bytes a line
        addd    #SCREEN
        tfr     d,u
        ldx     #_snow_line
        ldb     _line
        ldb     b,x                     ; the record for the line
        clra
        lslb
        rola
        ldx     #_snow_at
        ldd     d,x
        addd    #_snow_rec
        tfr     d,y
        lda     _seed                   ; a fresh start in the table
        lsla
        bcc     @noxor
        eora    #$1D
@noxor  sta     _seed
        anda    #$DF                    ; at most 223: the 32 bytes of a line stay inside it
        tfr     a,b
        ldx     #_snow_rand
        abx
@token  lda     ,y+
        beq     @next
        bmi     @full
        cmpa    #$40
        bhs     @skip
        tfr     a,b                     ; n bytes masked: new = old ^ ((old ^ static) & mask)
@mask   lda     ,x+
@mflip  eora    #0
        eora    ,u
        anda    ,y+
        eora    ,u
        sta     ,u+
        decb
        bne     @mask
        bra     @token
@skip   suba    #$40
        leau    a,u
        bra     @token
@full   anda    #$7F                    ; n bytes all static
        tfr     a,b
@all    lda     ,x+
@aflip  eora    #0
        sta     ,u+
        decb
        bne     @all
        bra     @token
@next   lda     _line
        adda    #2
        sta     _line
        cmpa    #SNOW_LINES
        lblo    @line
        puls    y,u
    }
}
