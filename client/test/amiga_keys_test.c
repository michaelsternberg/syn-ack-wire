/* Host tests for src/amiga/keys.c: an Amiga raw key (and what the console device made of it) to a platform.h key.
 *   make test-amiga-keys */
#include <stdio.h>
#include "../src/platform.h"
#include "../src/amiga/keys.h"

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

#define SHIFT 0x0001
#define CTRL  0x0008
#define LAMIGA 0x0040
#define RAMIGA 0x0080
#define REPEAT 0x0200

static void test_letters_upper(void)
{
    CHECK(amiga_key_translate(0x20, 0, 'a') == 'A');
    CHECK(amiga_key_translate(0x20, SHIFT, 'A') == 'A');
    CHECK(amiga_key_translate(0x35, 0, 'b') == 'B');
}
static void test_shifted_digit_passes(void)
{
    CHECK(amiga_key_translate(0x01, SHIFT, '!') == '!');
    CHECK(amiga_key_translate(0x01, 0, '1') == '1');
}
static void test_y_and_n_in_either_case(void)
{
    CHECK(amiga_key_translate(0x15, 0, 'y') == 'Y' && amiga_key_translate(0x15, SHIFT, 'Y') == 'Y');   /* the Y/N questions */
    CHECK(amiga_key_translate(0x36, 0, 'n') == 'N' && amiga_key_translate(0x36, SHIFT, 'N') == 'N');
}
static void test_function_keys(void)
{
    CHECK(amiga_key_translate(0x50, 0, 0) == K_HELP);      /* F1: the instructions */
    CHECK(amiga_key_translate(0x51, 0, 0) == K_OPTION);    /* F2: move hints */
    CHECK(amiga_key_translate(0x52, 0, 0) == AK_SOUND);    /* F3: sound, done by in_key */
    CHECK(amiga_key_translate(0x53, 0, 0) == AK_TABLE);    /* F4: cards or table, done by in_key */
}
static void test_help_key(void)
{
    CHECK(amiga_key_translate(0x5F, 0, 0) == K_HELP);
    CHECK(amiga_key_translate(0x3A, SHIFT, '?') == K_HELP);
}
static void test_backspace_and_del(void)
{
    CHECK(amiga_key_translate(0x41, 0, 8) == K_DELETE);
    CHECK(amiga_key_translate(0x46, 0, 127) == K_DELETE);
}
static void test_return_and_esc(void)
{
    CHECK(amiga_key_translate(0x44, 0, 13) == K_RETURN);
    CHECK(amiga_key_translate(0x43, 0, 13) == K_RETURN);   /* the numeric pad's Enter */
    CHECK(amiga_key_translate(0x45, 0, 27) == K_ESC);
}
static void test_arrows(void)
{
    CHECK(amiga_key_translate(0x4F, 0, 0) == '<');         /* cursor left: the instructions' previous page */
    CHECK(amiga_key_translate(0x4E, 0, 0) == '>');
    CHECK(amiga_key_translate(0x4C, 0, 0) == 0);           /* up and down mean nothing */
}
static void test_key_up_ignored(void)
{
    CHECK(amiga_key_translate(0x20 | 0x80, 0, 'a') == 0);
    CHECK(amiga_key_translate(0x44 | 0x80, 0, 13) == 0);
}
static void test_non_ascii_dropped(void)
{
    CHECK(amiga_key_translate(0x20, 0, (char)0xE4) == 0);  /* an umlaut from a foreign keymap */
    CHECK(amiga_key_translate(0x1E, 0, '{') == 0);         /* above $5F after upper-casing */
    CHECK(amiga_key_translate(0x20, 0, 'z') == 'Z');
}
static void test_chords_dropped(void)
{
    CHECK(amiga_key_translate(0x20, LAMIGA, 'a') == 0);    /* Amiga-key shortcuts are for the menus */
    CHECK(amiga_key_translate(0x20, RAMIGA, 'q') == 0);
    CHECK(amiga_key_translate(0x20, CTRL, 1) == 0);
}
static void test_repeat_passes(void)
{
    CHECK(amiga_key_translate(0x41, REPEAT, 8) == K_DELETE);
}

int main(void)
{
    RUN(test_letters_upper); RUN(test_shifted_digit_passes); RUN(test_function_keys); RUN(test_help_key);
    RUN(test_backspace_and_del); RUN(test_return_and_esc); RUN(test_y_and_n_in_either_case); RUN(test_arrows); RUN(test_key_up_ignored);
    RUN(test_non_ascii_dropped); RUN(test_chords_dropped); RUN(test_repeat_passes);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
