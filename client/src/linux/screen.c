/* The terminal for the linux client: see screen.h. */
#define _XOPEN_SOURCE_EXTENDED 1
#define _GNU_SOURCE 1
#include <curses.h>
#include <fcntl.h>
#include <stdio.h>
#include <wchar.h>
#include <langinfo.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "screen.h"

nl_cell nl_cells[NL_ROWS][NL_COLS];
nl_cell (*nl_draw)[NL_COLS] = nl_cells;

/* ---------------------------------------------------------------- the palette */
static const unsigned long palette[C_COUNT] = {
    0x262b33,                           /* C_BG: the screen's slate */
    0x1e1e1e,                           /* C_EMPTY: an empty node */
    0x000080,                           /* C_BAR: the title bar */
    0x5c0057,                           /* C_STRIP: the prompt strip */
    0x33ddee,                           /* C_CYAN */
    0xff55cc,                           /* C_MAGENTA */
    0x33ee55,                           /* C_GREEN */
    0xffd633,                           /* C_YELLOW */
    0xf0f0f0,                           /* C_WHITE */
    0x8a8f98,                           /* C_DIM */
    0x0a5aa8,                           /* C_BLUE: the ICE badge */
    0xff0000,                           /* C_RED */
    0x000000,                           /* C_BLACK */
    0x444a52,                           /* C_GREY: a spike */
    0x00ff88, 0xff0000, 0xffd633, 0x1ea0ff, 0xff8822, 0xff66aa, 0xaa88ff    /* the syndicates, in theme order */
};

unsigned long nl_rgb(unsigned char pal)
{
    return pal < C_COUNT ? palette[pal] : 0;
}

static unsigned dist(unsigned long a, unsigned long b)
{
    int dr = (int)(a >> 16 & 255) - (int)(b >> 16 & 255);
    int dg = (int)(a >> 8 & 255) - (int)(b >> 8 & 255);
    int db = (int)(a & 255) - (int)(b & 255);
    return (unsigned)(dr * dr + dg * dg + db * db);
}

unsigned char nl_rgb256(unsigned long rgb)
{
    static const unsigned char level[6] = { 0, 95, 135, 175, 215, 255 };
    unsigned long best_rgb = 0;
    unsigned best = ~0u, d;
    unsigned char r, g, b, idx = 16, k;
    for (r = 0; r < 6; ++r)
        for (g = 0; g < 6; ++g)
            for (b = 0; b < 6; ++b) {
                unsigned long c = (unsigned long)level[r] << 16 | (unsigned long)level[g] << 8 | level[b];
                if ((d = dist(rgb, c)) < best) {
                    best = d;
                    idx = (unsigned char)(16 + 36 * r + 6 * g + b);
                    best_rgb = c;
                }
            }
    (void)best_rgb;
    for (k = 0; k < 24; ++k) {
        unsigned char v = (unsigned char)(8 + 10 * k);
        if ((d = dist(rgb, (unsigned long)v * 0x010101UL)) < best) {
            best = d;
            idx = (unsigned char)(232 + k);
        }
    }
    return idx;
}

/* the terminal's own 16 colours, as xterm shows them, for a terminal that has 16 or 8 and no more */
static unsigned char nearest16(unsigned long rgb, int colors)
{
    static const unsigned long ansi[16] = {
        0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
        0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff
    };
    unsigned char k, best = 0;
    unsigned bd = ~0u;
    for (k = 0; k < (colors >= 16 ? 16 : 8); ++k)
        if (dist(rgb, ansi[k]) < bd) {
            bd = dist(rgb, ansi[k]);
            best = k;
        }
    return best;
}

unsigned int nl_ascii(unsigned int cp)
{
    if (cp < 0x80)
        return cp;
    switch (cp) {
    case 0x25B6: case 0x25BA: return '>';
    case 0xA5: return 'Y';
    case 0xB7: case 0x2022: return '.';
    case 0x2588: return '#';
    case 0x2591: case 0x2592: case 0x2593: return ':';
    case 0x2502: case 0x2551: return '|';
    case 0x2500: case 0x2550: return '-';
    }
    if (cp >= 0x2500 && cp <= 0x257F)   /* the remaining box drawing: corners, tees, crosses */
        return '+';
    return '?';
}

unsigned char nl_fit(int rows, int cols)
{
    return rows >= NL_ROWS && cols >= NL_COLS;
}

/* ---------------------------------------------------------------- the cells */
void nl_put(unsigned char r, unsigned char c, unsigned int ch, unsigned char fg, unsigned char bg, unsigned char attr)
{
    if (r >= NL_ROWS || c >= NL_COLS)
        return;
    nl_draw[r][c].ch = ch;
    nl_draw[r][c].fg = fg;
    nl_draw[r][c].bg = bg;
    nl_draw[r][c].attr = attr;
}

void nl_text(unsigned char r, unsigned char c, const char *s, unsigned char fg, unsigned char bg)
{
    for (; *s && c < NL_COLS; ++s, ++c)
        nl_put(r, c, (unsigned char)*s, fg, bg, 0);
}

void nl_fill(unsigned char r, unsigned char c, unsigned char w, unsigned char fg, unsigned char bg)
{
    for (; w && c < NL_COLS; --w, ++c)
        nl_put(r, c, ' ', fg, bg, 0);
}

void nl_clear(unsigned char fg, unsigned char bg)
{
    unsigned char r;
    for (r = 0; r < NL_ROWS; ++r)
        nl_fill(r, 0, NL_COLS, fg, bg);
}

/* ---------------------------------------------------------------- the terminal */
static unsigned char opened, utf8, can_colour, stop, tiny_shown;
static unsigned char term256[C_COUNT];
static nl_cell drawn[NL_ROWS][NL_COLS];
static short pair_of[C_COUNT][C_COUNT];
static short next_pair = 1;

static void forget(void)
{
    memset(drawn, 0xFF, sizeof drawn);
}

static void on_signal(int sig)
{
    (void)sig;
    if (stop) {                         /* a second one: the game is stuck, so go now */
        endwin();
        _exit(130);
    }
    stop = 1;
}

void nl_restore(void)
{
    if (opened)
        endwin();
    opened = 0;
}

void nl_open(void)
{
    struct sigaction sa;
    struct termios t;
    unsigned char k;
    if (opened)
        return;
    setlocale(LC_ALL, "");
    utf8 = strstr(nl_langinfo(CODESET), "UTF-8") != NULL || strstr(nl_langinfo(CODESET), "utf8") != NULL;
    initscr();
    {                                   /* fujinet-nio-lib prints its complaints to stderr: not over the screen */
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, 2);
            close(null);
        }
    }
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    if (tcgetattr(0, &t) == 0) {        /* Ctrl-S and Ctrl-Q are keys, not flow control; endwin puts the old mode back */
        t.c_iflag &= ~(IXON | IXOFF);
        tcsetattr(0, TCSANOW, &t);
        def_prog_mode();                /* so that resuming after Ctrl-Z keeps flow control off */
    }
    can_colour = has_colors();
    if (can_colour) {
        start_color();
        use_default_colors();
        for (k = 0; k < C_COUNT; ++k)
            term256[k] = COLORS >= 256 ? nl_rgb256(palette[k]) : nearest16(palette[k], COLORS);
    }
    opened = 1;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    atexit(nl_restore);
    forget();
}

unsigned char nl_can_colour(void)
{
    return opened && can_colour;
}

int nl_read(void)
{
    int c = getch();
    if (c == KEY_RESIZE) {
        forget();
        clear();
        return -1;
    }
    return c == ERR ? -1 : c;
}

/* the colour pair for a cell: made when first used; 0 (the default) if the terminal has run out */
static short pair_for(unsigned char fg, unsigned char bg)
{
    if (!pair_of[fg][bg]) {
        if (next_pair >= COLOR_PAIRS || next_pair >= 32000)
            return 0;
        init_pair(next_pair, term256[fg], term256[bg]);
        pair_of[fg][bg] = next_pair++;
    }
    return pair_of[fg][bg];
}

void nl_refresh(void)
{
    int r, c, oy, ox;
    if (stop) {
        nl_restore();
        exit(130);
    }
    if (!opened)
        return;
    if (!nl_fit(LINES, COLS)) {
        erase();
        mvaddnstr(LINES / 2, 0, "TERMINAL TOO SMALL: NEED 80x24", COLS);
        refresh();
        forget();
        tiny_shown = 1;
        return;
    }
    if (tiny_shown) {
        clear();
        tiny_shown = 0;
    }
    oy = (LINES - NL_ROWS) / 2;
    ox = (COLS - NL_COLS) / 2;
    for (r = 0; r < NL_ROWS; ++r)
        for (c = 0; c < NL_COLS; ++c) {
            const nl_cell *cell = &nl_cells[r][c];
            attr_t at = 0;
            short pair = 0;
            cchar_t cc;
            wchar_t w[2];
            if (!memcmp(cell, &drawn[r][c], sizeof *cell))
                continue;
            drawn[r][c] = *cell;
            if (can_colour) {
                pair = pair_for(cell->fg, cell->bg);
                if (COLORS < 16 && cell->fg != C_BG && (cell->attr & NL_BOLD))
                    at |= A_BOLD;
            } else if (cell->bg != C_BG && cell->bg != C_EMPTY) {
                at |= A_REVERSE;
            }
            if (cell->attr & NL_BOLD)
                at |= A_BOLD;
            if (cell->attr & NL_BLINK)
                at |= A_BLINK;
            if (cell->attr & NL_DIM)
                at |= A_DIM;
            w[0] = (wchar_t)(utf8 ? cell->ch : nl_ascii(cell->ch));
            w[1] = 0;
            setcchar(&cc, w, at, pair, NULL);
            mvadd_wch(oy + r, ox + c, &cc);
        }
    refresh();
}
