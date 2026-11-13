/* Host tests for src/linux/screen.c's pure parts: the cells, the palette and the 256-colour mapping.
 *   make test-linux-screen */
#include <stdio.h>
#include <string.h>
#include "../src/linux/screen.h"

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static void test_fit_needs_80_by_24(void)
{
    CHECK(nl_fit(24, 80) == 1);
    CHECK(nl_fit(23, 80) == 0);
    CHECK(nl_fit(24, 79) == 0);
    CHECK(nl_fit(25, 80) == 1);
    CHECK(nl_fit(60, 200) == 1);
    CHECK(nl_fit(0, 0) == 0);
}
static void test_rgb_to_256(void)
{
    unsigned long v;
    CHECK(nl_rgb256(0x000000) == 16);
    CHECK(nl_rgb256(0xffffff) == 231);
    CHECK(nl_rgb256(0x00ff88) == 48);
    CHECK(nl_rgb256(0xff8822) == 208);
    CHECK(nl_rgb256(0xaa88ff) == 141);
    CHECK(nl_rgb256(0x1e1e1e) == 234);
    for (v = 0; v < 0x1000000UL; v += 4099)         /* never the terminal's own 16 colours: those are the user's theme */
        CHECK(nl_rgb256(v) >= 16);
}
static void test_palette(void)
{
    CHECK(nl_rgb(C_BG) == 0x262b33);
    CHECK(nl_rgb(C_BAR) == 0x000080);
    CHECK(nl_rgb(C_SYN0) == 0x00ff88);
    CHECK(nl_rgb(C_SYN0 + 6) == 0xaa88ff);
    CHECK(C_SYN0 + 7 == C_COUNT);
}
static void test_ascii_stand_ins(void)
{
    CHECK(nl_ascii(0x25B6) == '>');
    CHECK(nl_ascii(0xA5) == 'Y');
    CHECK(nl_ascii(0x2588) == '#');
    CHECK(nl_ascii(0x250C) == '+');
    CHECK(nl_ascii(0x2502) == '|');
    CHECK(nl_ascii(0x2500) == '-');
    CHECK(nl_ascii(0xB7) == '.');
    CHECK(nl_ascii(0x2591) == ':');
    CHECK(nl_ascii(0x4E2D) == '?');
    CHECK(nl_ascii('A') == 'A');
}
static void test_cells_are_written_exactly(void)
{
    nl_clear(C_WHITE, C_BG);
    CHECK(nl_cells[3][4].ch == ' ' && nl_cells[3][4].fg == C_WHITE && nl_cells[3][4].bg == C_BG && nl_cells[3][4].attr == 0);
    nl_put(2, 5, 'x', C_CYAN, C_BAR, NL_BOLD);
    CHECK(nl_cells[2][5].ch == 'x' && nl_cells[2][5].fg == C_CYAN && nl_cells[2][5].bg == C_BAR && nl_cells[2][5].attr == NL_BOLD);
    nl_text(4, 10, "HIVE", C_GREEN, C_STRIP);
    CHECK(nl_cells[4][10].ch == 'H' && nl_cells[4][13].ch == 'E' && nl_cells[4][13].fg == C_GREEN && nl_cells[4][14].ch == ' ');
    nl_fill(6, 0, 3, C_DIM, C_EMPTY);
    CHECK(nl_cells[6][2].bg == C_EMPTY && nl_cells[6][3].bg == C_BG);
}
static void test_writes_clip_at_the_edges(void)
{
    nl_clear(C_WHITE, C_BG);
    nl_text(0, 78, "ABCD", C_WHITE, C_BG);                 /* only AB fit */
    CHECK(nl_cells[0][78].ch == 'A' && nl_cells[0][79].ch == 'B');
    nl_put(24, 0, 'z', C_WHITE, C_BG, 0);                  /* off the screen: nothing, and no crash */
    nl_put(0, 80, 'z', C_WHITE, C_BG, 0);
    nl_fill(23, 78, 10, C_DIM, C_EMPTY);
    CHECK(nl_cells[23][79].bg == C_EMPTY && nl_cells[23][77].bg == C_BG);
}

int main(void)
{
    RUN(test_fit_needs_80_by_24); RUN(test_rgb_to_256); RUN(test_palette); RUN(test_ascii_stand_ins);
    RUN(test_cells_are_written_exactly); RUN(test_writes_clip_at_the_edges);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
