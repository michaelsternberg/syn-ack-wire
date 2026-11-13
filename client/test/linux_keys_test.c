/* Host tests for src/linux/keys.c: curses keys to the game's keys (as src/msdos/input.c).
 *   make test-linux-keys */
#include <stdio.h>
#include <curses.h>
#include "../src/platform.h"
#include "../src/linux/keys.h"

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static void test_function_keys(void)
{
    CHECK(nl_keymap(KEY_F(1)) == K_HELP);
    CHECK(nl_keymap('?') == K_HELP);
    CHECK(nl_keymap(KEY_F(2)) == K_OPTION);
    CHECK(nl_keymap(KEY_F(3)) == NL_KEY_SOUND);
    CHECK(nl_keymap(KEY_F(4)) == 0);                    /* no width or colour toggles */
    CHECK(nl_keymap(KEY_F(5)) == 0);
    CHECK(nl_keymap(KEY_F(9)) == 0);
}
static void test_tab_hints_and_ctrl_s_sound(void)
{
    CHECK(nl_keymap(9) == K_OPTION);
    CHECK(nl_keymap(KEY_BTAB) == 0);
    CHECK(nl_keymap(19) == NL_KEY_SOUND);
}
static void test_editing_and_paging(void)
{
    CHECK(nl_keymap(KEY_BACKSPACE) == K_DELETE);
    CHECK(nl_keymap(127) == K_DELETE);
    CHECK(nl_keymap(8) == K_DELETE);
    CHECK(nl_keymap(KEY_DC) == K_DELETE);
    CHECK(nl_keymap(KEY_LEFT) == '<');
    CHECK(nl_keymap(KEY_RIGHT) == '>');
    CHECK(nl_keymap(KEY_UP) == K_UP);
    CHECK(nl_keymap(KEY_DOWN) == K_DOWN);
}
static void test_enter_and_escape(void)
{
    CHECK(nl_keymap('\n') == K_RETURN);
    CHECK(nl_keymap('\r') == K_RETURN);
    CHECK(nl_keymap(KEY_ENTER) == K_RETURN);
    CHECK(nl_keymap(27) == K_ESC);
}
static void test_letters_are_upper_case(void)
{
    CHECK(nl_keymap('q') == 'Q');
    CHECK(nl_keymap('Q') == 'Q');
    CHECK(nl_keymap('z') == 'Z');
    CHECK(nl_keymap('5') == '5');
    CHECK(nl_keymap(' ') == ' ');
    CHECK(nl_keymap('<') == '<');
}
static void test_everything_else_is_nothing(void)
{
    CHECK(nl_keymap(0x1F) == 0);
    CHECK(nl_keymap(0x80) == 0);
    CHECK(nl_keymap(KEY_RESIZE) == 0);
    CHECK(nl_keymap(-1) == 0);
    CHECK(nl_keymap(KEY_MOUSE) == 0);
}

int main(void)
{
    RUN(test_function_keys); RUN(test_tab_hints_and_ctrl_s_sound); RUN(test_editing_and_paging); RUN(test_enter_and_escape);
    RUN(test_letters_are_upper_case); RUN(test_everything_else_is_nothing);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
