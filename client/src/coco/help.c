/* The instruction pages for the CoCo 1/2. They are one Disk BASIC file,
 * SYNACK.HLP, made by tools/help_coco.py: page n is the 6144 bytes from
 * (n-1) * 6144, an image of the PMODE 4 screen, read straight onto it, 24
 * sectors through Disk BASIC's DSKCON (which HDB-DOS sends to FujiNet over
 * DriveWire). No file code: the directory entry and the FAT are read once, and
 * the FAT's granule chain says where each sector is. Pages are drawn green on
 * black; black on green (display_colours) turns every byte over.
 *
 * While a page is up the game goes on writing txt_cells; display_text.c draws
 * nothing meanwhile, and closing draws it all. '>' past the last page finds the
 * file too short and stays put. */
#include <coco.h>
#include "../platform.h"
#include "../display.h"
#include "text.h"

#define SCREEN      ((unsigned char *)0x0E00)
#define PAGE_SECS   24                  /* 6144 / 256 */
#define GRAN_SECS   9
#define DIR_TRACK   17
#define DEFDRV      (*(unsigned char *)0x095A)  /* Disk BASIC's default drive */

/* DSKCON's parameters, at $EA-$F0 */
#define DCOPC       (*(unsigned char *)0x00EA)
#define DCDRV       (*(unsigned char *)0x00EB)
#define DCTRK       (*(unsigned char *)0x00EC)
#define DCSEC       (*(unsigned char *)0x00ED)
#define DCBPT       (*(unsigned char **)0x00EE)
#define DCSTA       (*(unsigned char *)0x00F0)

unsigned char helping;
extern unsigned char green;             /* display_text.c: black on green */

static unsigned char page;              /* the page on screen; 0 = none yet */
static unsigned char gran0 = 0xFF;      /* SYNACK.HLP's first granule; 0xFE = no file */
static unsigned char fat[68];
static unsigned char nofile[2 * TXT_COLS];   /* a row of cells saying so, and a blank one */
static const char nofile_text[] = "NO HELP FILE (SYNACK.HLP). BREAK: BACK";
static const char name[11] = { 'S', 'Y', 'N', 'A', 'C', 'K', ' ', ' ', 'H', 'L', 'P' };

/* Every pixel turned over: a page in the other colours (display_colours, while one is up). */
void help_flip(void)
{
    unsigned char *p;

    for (p = SCREEN; p < SCREEN + 6144; ++p)
        *p = (unsigned char)~*p;
}

/* One sector into buf; 1 if it read. */
static unsigned char sector(unsigned char *buf, unsigned char track, unsigned char sec)
{
    DCOPC = 2;                          /* read */
    DCDRV = DEFDRV;
    DCTRK = track;
    DCSEC = sec;
    DCBPT = buf;
    asm {
        jsr     [$C004]                 ; DSKCON
    }
    return DCSTA == 0;
}

/* The directory entry's first granule, and the FAT; gran0 = 0xFE if there's no file. */
static void find(void)
{
    unsigned char s, e, k, *d;

    gran0 = 0xFE;
    if (!sector(SCREEN, DIR_TRACK, 2))
        return;
    memcpy(fat, SCREEN, sizeof fat);
    for (s = 3; s <= 11; ++s) {
        if (!sector(SCREEN, DIR_TRACK, s))
            return;
        for (e = 0; e < 8; ++e) {
            d = SCREEN + (e << 5);
            if (*d == 0xFF)             /* the end of the directory */
                return;
            for (k = 0; k < 11 && d[k] == name[k]; ++k)
                ;
            if (k == 11) {
                gran0 = d[13];
                return;
            }
        }
    }
}

/* Where sector idx of the file is: its track in trk, the result its sector;
 * 0 if the file isn't that long. */
static unsigned char trk;

static unsigned char where(unsigned idx)
{
    unsigned char g = gran0, n, s, last;

    for (n = (unsigned char)(idx / GRAN_SECS); n; --n) {
        g = fat[g];
        if (g >= 0x44)                  /* past the last granule */
            return 0;
    }
    s = (unsigned char)(idx % GRAN_SECS);
    last = fat[g];
    if (last >= 0xC0 && s >= (last & 0x3F))         /* past the last granule's sectors */
        return 0;
    trk = (unsigned char)((g >> 1) + (g >= 34 ? 1 : 0));    /* track 17 is the directory's */
    return (unsigned char)((g & 1) * GRAN_SECS + 1 + s);
}

/* Page p onto the screen, if the file has all of it; 1 if it does. */
static unsigned char load(unsigned char p)
{
    unsigned char k, s;
    unsigned idx = (unsigned)(p - 1) * PAGE_SECS;
    unsigned char *dst = SCREEN;

    if (gran0 == 0xFF)
        find();
    if (gran0 >= 0x44 || !where(idx + PAGE_SECS - 1))
        return 0;
    for (k = 0; k < PAGE_SECS; ++k, ++idx, dst += 256) {
        s = where(idx);
        if (!sector(dst, trk, s))
            return 0;
    }
    if (green)
        help_flip();
    page = p;
    return 1;
}

void help_open(void)
{
    unsigned char k;

    helping = 1;
    if (load(page ? page : 1))
        return;
    memset(nofile, ' ', sizeof nofile);
    for (k = 0; nofile_text[k]; ++k)
        nofile[k] = (unsigned char)nofile_text[k] | TXT_INV;
    txt_src = nofile;
    txt_clear();
    txt_row(0);
}

unsigned char __fastcall__ help_key(unsigned char k)
{
    if (k == K_ESC || k == K_RETURN || k == K_HELP) {
        helping = 0;
        txt_src = txt_cells;
        txt_colours(green);
        return 0;
    }
    if (txt_src == nofile)
        return 1;
    if (k == '>')
        load((unsigned char)(page + 1));
    else if ((k == '<' || k == K_DELETE) && page > 1)
        load((unsigned char)(page - 1));
    return 1;
}
