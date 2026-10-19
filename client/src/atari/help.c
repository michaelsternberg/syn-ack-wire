/* Instructions for the Atari: HELP01.TXT, HELP02.TXT... on the game disk, read
 * through DOS. Each file is one screen of ATASCII: up to 23 lines of up to 40
 * characters, each ended by EOL ($9B); the bottom row is the page bar. Every
 * byte except EOL is shown as its glyph, so graphics characters and inverse
 * video work as typed. Pages are counted the first time, by trying to open
 * each name in turn, so adding a page needs no code change.
 *
 *   <PREV   PAGE 2 OF 5 ESC:EXIT    NEXT>      (row 23, inverse) */
#include <string.h>
#include "../platform.h"
#include "../display.h"

#define SAVMSC  (*(unsigned char **)0x58)
#define ROWS    23
#define EOL     0x9B
#define INV     0x80

unsigned char __fastcall__ cio_open(const char *name);  /* cio.s */
unsigned char cio_get(void);
unsigned char cio_close(void);
extern unsigned char cio_byte;

static char name[] = "D:HELP01.TXT\x9B";
static unsigned char pages, page, r, c, b, v;
static unsigned char *p, *scr, *barp;    /* screen memory, its bottom row */

static void set_name(unsigned char n)
{
    for (b = '0'; n >= 10; ++b)
        n -= 10;
    name[6] = b;
    name[7] = '0' + n;
}

static unsigned char exists(unsigned char n)
{
    set_name(n);
    v = cio_open(name);
    cio_close();
    return v < 128;
}

/* text into the page bar at p, inverse */
static void bar(const char *s)
{
    while (*s)
        *p++ = (*s++ - 0x20) | INV;
}

static void bar_num(unsigned char n)
{
    for (b = 0; n >= 10; ++b)
        n -= 10;
    if (b)
        *p++ = ('0' + b - 0x20) | INV;
    *p++ = ('0' + n - 0x20) | INV;
}

static void at_bar(unsigned char col)
{
    p = barp + col;
}

static void page_text(void)
{
    p = scr;
    r = c = 0;
    set_name(page);
    if (cio_open(name) < 128) {
        while (cio_get() < 128) {
            b = cio_byte;
            if (b == EOL) {
                if (++r == ROWS)
                    break;
                p += 40;
                c = 0;
                continue;
            }
            if (c < 40) {                   /* ATASCII -> screen code */
                v = b & 0x7F;
                if (v < 0x20)
                    v += 0x40;
                else if (v < 0x60)
                    v -= 0x20;
                p[c] = v | (b & INV);
            }
            ++c;
        }
    }
    cio_close();
}

static void draw(void)
{
    memset(scr, 0, 40 * ROWS);
    memset(barp, INV, 40);
    if (pages) {
        page_text();
        if (page > 1) {
            at_bar(1);
            bar("<PREV");
        }
        at_bar(9);
        bar("PAGE ");
        bar_num(page);
        bar(" OF ");
        bar_num(pages);
        if (page < pages) {
            at_bar(34);
            bar("NEXT>");
        }
    } else {
        at_bar(1);
        bar("NO HELP DISK IN D1");
    }
    at_bar(22);
    bar("ESC:EXIT");
}

void help_show(unsigned char (*key)(void))
{
    scr = SAVMSC;
    barp = scr + 40 * ROWS;
    if (!pages)                         /* count once; retry while there's no disk */
        while (pages < 99 && exists(pages + 1))
            ++pages;
    page = 1;
    draw();
    for (;;) {
        b = key();
        if ((b == '>' || b == '*') && page < pages) {
            ++page;
            draw();
        } else if ((b == '<' || b == '+') && page > 1) {
            --page;
            draw();
        } else if (b == K_ESC || b == K_RETURN || b == K_HELP) {
            return;
        }
    }
}
