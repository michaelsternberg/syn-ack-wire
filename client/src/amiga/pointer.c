/* The mouse pointer: the mockups' arrow as a Kickstart 1.3 sprite image (two bitplanes, so colours 1-3 of the sprite's
 * registers 17-19 in palette.h). The data is built into chip RAM, which the sprite DMA reads, when the window opens. */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "amiga.h"

#define ROWS 14

/* X: the outline (sprite colour 1), O: the fill (2) */
static const char *const arrow[ROWS] = {
    "X.........", "XX........", "XOX.......", "XOOX......", "XOOOX.....", "XOOOOX....", "XOOOOOX...",
    "XOOOOOOX..", "XOOOOXXXX.", "XOXOOX....", "XX.XOOX...", "X..XOOX...", "....XOOX..", "....XXXX..",
};

/* The busy pointer: an hourglass, X the glass and O the sand */
static const char *const glass[ROWS] = {
    "XXXXXXXXXX", "XOOOOOOOOX", ".XOOOOOOX.", "..XOOOOX..", "...XOOX...", "....XX....", "....XX....",
    "...XOOX...", "..XOXXOX..", ".XOOOOOOX.", "XOOOOOOOOX", "XXXXXXXXXX", "..........", "..........",
};

static UWORD *image, *busy_image;
static struct Window *shown;            /* the window the arrow was set on, for the hourglass to go back to */
static unsigned char busy_depth;
#define IMAGE_BYTES ((2 + ROWS * 2 + 2) * 2)

static void paint(UWORD *img, const char *const *rows)
{
    unsigned char r, c;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; rows[r][c]; ++c) {
            if (rows[r][c] == 'X')
                img[2 + r * 2] |= 0x8000 >> c;
            else if (rows[r][c] == 'O')
                img[2 + r * 2 + 1] |= 0x8000 >> c;
        }
}

void amiga_pointer_set(struct Window *w)
{
    if (!image)
        image = (UWORD *)AllocMem(IMAGE_BYTES, MEMF_CHIP | MEMF_CLEAR);
    if (!image)
        return;                         /* the system's pointer stays */
    paint(image, arrow);
    shown = w;
    SetPointer(w, image, ROWS, 16, 0, 0);
}

/* Calls nest: the arrow comes back when the last one is over. Without a window, or without chip RAM for the hourglass,
 * nothing shows and nothing is lost. */
void pointer_busy(unsigned char on)
{
    if (on) {
        if (busy_depth++ || !shown)
            return;
        if (!busy_image) {
            busy_image = (UWORD *)AllocMem(IMAGE_BYTES, MEMF_CHIP | MEMF_CLEAR);
            if (busy_image)
                paint(busy_image, glass);
        }
        if (busy_image)
            SetPointer(shown, busy_image, ROWS, 16, -5, -6);   /* the hot spot at the hourglass's middle */
    } else if (busy_depth && !--busy_depth && shown && image) {
        SetPointer(shown, image, ROWS, 16, 0, 0);
    }
}

void amiga_pointer_clear(struct Window *w)
{
    if (w)
        ClearPointer(w);
    shown = 0;
    busy_depth = 0;
    if (busy_image)
        FreeMem(busy_image, IMAGE_BYTES);
    busy_image = 0;
    if (image)
        FreeMem(image, IMAGE_BYTES);
    image = 0;
}
