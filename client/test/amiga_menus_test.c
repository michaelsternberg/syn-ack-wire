/* The menu strip's table and what each pick stands for: pure, so tested on the host. */
#include <stdio.h>
#include <string.h>
#include "../src/platform.h"
#include "../src/amiga/keys.h"
#include "regions_tab.h"
#include "../src/amiga/menudef.h"

static int ran, failures;
#define CHECK(c) do { if (!(c)) { ++failures; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static void test_three_menus_in_order(void)
{
    CHECK(menu_count() == 3);
    CHECK(strcmp(menu_def(0)->name, "PROJECT") == 0 && strcmp(menu_def(1)->name, "GAME") == 0 && strcmp(menu_def(2)->name, "REGION") == 0);
    CHECK(menu_def(3) == 0);
}
static void test_project_and_game_picks_are_the_keys_they_stand_for(void)
{
    CHECK(menu_item_key(0, 0) == K_HELP);               /* Instructions */
    CHECK(menu_item_key(0, 1) == K_ESC);                /* Quit: as Esc, which asks first */
    CHECK(menu_item_key(1, 0) == K_OPTION);             /* Move hints */
    CHECK(menu_item_key(1, 1) == AK_SOUND);
    CHECK(menu_item_key(1, 2) == AK_TABLE);
}
static void test_each_region_is_one_item_with_its_own_pick(void)
{
    int n;
    CHECK(menu_def(2)->n == NREGIONS);
    for (n = 0; n < NREGIONS; ++n) {
        CHECK(menu_item_key(2, n) == AK_REGION + n);
        CHECK(strcmp(menu_def(2)->items[n].label, regions[n].name) == 0);
    }
}
static void test_no_region_pick_is_a_game_key(void)
{
    int n, c;
    for (n = 0; n < NREGIONS; ++n) {
        unsigned char k = menu_item_key(2, n);
        CHECK(ak_region(k) == n);       /* it is a Region pick ... */
        CHECK(k != K_ESC && k != K_RETURN && k != K_DELETE && k != K_OPTION && k != K_HELP);
        CHECK(!(k >= 0x20 && k <= 0x5F));
    }
    for (c = 0; c < 256; ++c)           /* ... and nothing else the game gets is */
        if (c == K_ESC || c == K_RETURN || c == K_DELETE || c == K_OPTION || c == K_HELP || (c >= 0x20 && c <= 0x5F))
            CHECK(ak_region((unsigned char)c) < 0);
}
static void test_nothing_past_the_ends(void)
{
    CHECK(menu_item_key(0, 2) == 0 && menu_item_key(1, 3) == 0 && menu_item_key(2, NREGIONS) == 0);
    CHECK(menu_item_key(3, 0) == 0 && menu_item_key(31, 63) == 0);
}
static void test_keys_do_not_collide(void)
{
    int m, i, a, b;
    for (m = 0; m < 3; ++m)
        for (i = 0; i < menu_def(m)->n; ++i) {
            unsigned char k = menu_item_key(m, i);
            CHECK(k != 0 && k != '\r' && k != 0x7F);
            for (a = 0; a < 3; ++a)
                for (b = 0; b < menu_def(a)->n; ++b)
                    CHECK((a == m && b == i) || menu_item_key(a, b) != k);
        }
}
static void test_command_keys_are_unique_letters(void)
{
    int m, i, a, b;
    for (m = 0; m < 3; ++m)
        for (i = 0; i < menu_def(m)->n; ++i) {
            char c = menu_def(m)->items[i].commkey;
            if (!c) continue;
            for (a = 0; a < 3; ++a)
                for (b = 0; b < menu_def(a)->n; ++b)
                    CHECK((a == m && b == i) || menu_def(a)->items[b].commkey != c);
        }
}

int main(void)
{
    RUN(test_three_menus_in_order); RUN(test_project_and_game_picks_are_the_keys_they_stand_for);
    RUN(test_each_region_is_one_item_with_its_own_pick); RUN(test_no_region_pick_is_a_game_key); RUN(test_nothing_past_the_ends);
    RUN(test_keys_do_not_collide); RUN(test_command_keys_are_unique_letters);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
