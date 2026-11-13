/* The terminal for the linux client: the display draws into nl_cells (a code point, two palette colours and some
 * attributes a cell), and this file paints it with ncursesw in the terminal's 256 colours (the nearest of 16 or 8
 * where that is all there is). The pure parts (nl_fit, nl_rgb256, nl_rgb, nl_ascii and the cell writers) are tested
 * on the host: make test-linux-screen. */
#ifndef NL_SCREEN_H
#define NL_SCREEN_H

#define NL_ROWS 24
#define NL_COLS 80

/* palette indices: the colours live in one table in screen.c */
enum {
    C_BG, C_EMPTY, C_BAR, C_STRIP, C_CYAN, C_MAGENTA, C_GREEN, C_YELLOW, C_WHITE, C_DIM, C_BLUE, C_RED, C_BLACK, C_GREY,
    C_SYN0,                             /* the seven syndicates: C_SYN0 + syndicate id */
    C_COUNT = C_SYN0 + 7
};

#define NL_BOLD  1
#define NL_BLINK 2
#define NL_DIM   4

typedef struct {
    unsigned int ch;                    /* a Unicode code point */
    unsigned char fg, bg, attr;         /* palette indices, NL_* */
} nl_cell;

extern nl_cell nl_cells[NL_ROWS][NL_COLS];     /* what the terminal shows */
extern nl_cell (*nl_draw)[NL_COLS];            /* where the writers below draw: nl_cells, or behind the help panel */

/* writers: off-screen parts are dropped */
void nl_put(unsigned char r, unsigned char c, unsigned int ch, unsigned char fg, unsigned char bg, unsigned char attr);
void nl_text(unsigned char r, unsigned char c, const char *s, unsigned char fg, unsigned char bg);   /* ASCII */
void nl_fill(unsigned char r, unsigned char c, unsigned char w, unsigned char fg, unsigned char bg);  /* spaces */
void nl_clear(unsigned char fg, unsigned char bg);

/* pure */
unsigned char nl_fit(int rows, int cols);       /* 1 if the terminal holds the 80x24 screen */
unsigned long nl_rgb(unsigned char pal);        /* a palette colour's 0xRRGGBB */
unsigned char nl_rgb256(unsigned long rgb);     /* the nearest xterm colour from 16 to 255 */
unsigned int nl_ascii(unsigned int cp);         /* an ASCII stand-in for a code point, for a terminal without UTF-8 */

/* the terminal */
void nl_open(void);                     /* curses on: raw keys (Ctrl-S arrives), no echo, hidden cursor; stderr to /dev/null */
void nl_refresh(void);                  /* paint what changed (centred), or say the terminal is too small; acts on a signal */
void nl_restore(void);                  /* the terminal as it was; safe to call again */
unsigned char nl_can_colour(void);
int nl_read(void);                      /* one curses key, or -1 if none (a resize repaints and returns -1) */

#endif
