/* Host tests for src/amiga/palcycle.c: the rotation of a run of palette entries (the title's neon). */
#include <stdio.h>
#include <string.h>
#include "../src/amiga/palcycle.h"

static int ran, failures;
#define CHECK(c) do { if (!(c)) { ++failures; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static void test_one_step_moves_every_entry_up_by_one(void)
{
    unsigned short p[7] = { 1, 2, 3, 4, 5, 6, 7 };
    palcycle(p, 7, 1);
    CHECK(p[0] == 7 && p[1] == 1 && p[6] == 6);
}
static void test_a_full_turn_returns_the_original(void)
{
    unsigned short p[7] = { 1, 2, 3, 4, 5, 6, 7 }, o[7] = { 1, 2, 3, 4, 5, 6, 7 };
    unsigned char i;
    for (i = 0; i < 7; ++i)
        palcycle(p, 7, 1);
    CHECK(memcmp(p, o, sizeof p) == 0);
    palcycle(p, 7, 14);                 /* two turns in one go */
    CHECK(memcmp(p, o, sizeof p) == 0);
}
static void test_one_and_two_entries(void)
{
    unsigned short a[1] = { 9 }, b[2] = { 1, 2 };
    palcycle(a, 1, 5);
    CHECK(a[0] == 9);                   /* nothing to rotate */
    palcycle(b, 2, 1);
    CHECK(b[0] == 2 && b[1] == 1);
}
static void test_entries_outside_the_run_are_untouched(void)
{
    unsigned short p[9] = { 100, 1, 2, 3, 4, 5, 6, 7, 200 };
    palcycle(p + 1, 7, 3);
    CHECK(p[0] == 100 && p[8] == 200);
    CHECK(p[1] == 5 && p[2] == 6 && p[3] == 7 && p[4] == 1);
    palcycle(p + 1, 0, 3);              /* n = 0: nothing, no crash */
    CHECK(p[0] == 100 && p[8] == 200);
}

int main(void)
{
    RUN(test_one_step_moves_every_entry_up_by_one); RUN(test_a_full_turn_returns_the_original);
    RUN(test_one_and_two_entries); RUN(test_entries_outside_the_run_are_untouched);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
