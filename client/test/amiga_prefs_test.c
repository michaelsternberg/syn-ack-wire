/* Host tests for src/amiga/prefs.c: the saved handle's file, S:SynAckWire.prefs.
 *   make test-amiga-prefs */
#include <stdio.h>
#include <string.h>
#include "../src/amiga/prefs.h"

static int failures, ran;
static struct prefs_set def;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static void test_roundtrip(void)
{
    char file[64], handle[9];
    unsigned char n = prefs_format("NEON", "", &def, file);
    CHECK(strncmp(file, "HANDLE=NEON\n", 12) == 0 && n == 12 + 30);
    CHECK(prefs_parse(file, n, handle) == 4);
    CHECK(strcmp(handle, "NEON") == 0);
}
static void test_lowercase_upcased(void)
{
    char handle[9];
    CHECK(prefs_parse("HANDLE=mona\n", 12, handle) == 4);
    CHECK(strcmp(handle, "MONA") == 0);
    CHECK(prefs_parse("handle=case\n", 12, handle) == 4);      /* the key too */
    CHECK(strcmp(handle, "CASE") == 0);
}
static void test_bad_chars_dropped(void)
{
    char handle[9];
    CHECK(prefs_parse("HANDLE=a b{c}~d\r\n", 17, handle) == 4);  /* space and above Z go; CR ends it */
    CHECK(strcmp(handle, "ABCD") == 0);
}
static void test_missing_file_gives_empty(void)
{
    char handle[9] = "JUNK";
    CHECK(prefs_parse("", 0, handle) == 0);
    CHECK(handle[0] == 0);
    CHECK(prefs_parse("OTHER=1\n", 8, handle) == 0);            /* no HANDLE line */
    CHECK(handle[0] == 0);
}
static void test_overlong_truncated_to_8(void)
{
    char handle[9], file[64];
    CHECK(prefs_parse("HANDLE=ABCDEFGHIJKL\n", 20, handle) == 8);
    CHECK(strcmp(handle, "ABCDEFGH") == 0);
    CHECK(prefs_format("ABCDEFGHIJKL", "", &def, file) == 16 + 30);           /* HANDLE= + 8 + newline */
}
static void test_handle_line_found_among_others(void)
{
    char handle[9];
    CHECK(prefs_parse("X=1\nHANDLE=ZED\nY=2\n", 19, handle) == 3);
    CHECK(strcmp(handle, "ZED") == 0);
}

static void test_server_line_found(void)
{
    char url[SERVER_MAX + 1];
    const char *f = "HANDLE=ZED\nSERVER=TCP://192.168.1.86:6599/\n";
    CHECK(prefs_server(f, (unsigned char)strlen(f), url) == 24);
    CHECK(strcmp(url, "TCP://192.168.1.86:6599/") == 0);
}
static void test_server_accepts_the_n_prefix_and_lowercase_key(void)
{
    char url[SERVER_MAX + 1];
    const char *f = "server=N:TCP://mark.local:6599/\r\n";
    CHECK(prefs_server(f, (unsigned char)strlen(f), url) == 22);
    CHECK(strcmp(url, "TCP://mark.local:6599/") == 0);          /* host case is kept: it is a name */
}
static void test_server_must_be_tcp(void)
{
    char url[SERVER_MAX + 1] = "JUNK";
    const char *f = "SERVER=HTTP://x/\n";
    CHECK(prefs_server(f, (unsigned char)strlen(f), url) == 0 && url[0] == 0);
    CHECK(prefs_server("", 0, url) == 0);
    CHECK(prefs_server("HANDLE=A\n", 9, url) == 0);
}
static void test_server_too_long_is_ignored(void)
{
    char url[SERVER_MAX + 1], f[200] = "SERVER=TCP://";
    memset(f + 13, 'a', 100);
    memcpy(f + 113, "/\n", 3);
    CHECK(prefs_server(f, 115, url) == 0);                       /* never a truncated, wrong address */
}
static void test_format_keeps_the_server(void)
{
    char file[160], handle[9], url[SERVER_MAX + 1];
    unsigned char n = prefs_format("NEON", "TCP://h:1/", &def, file);
    CHECK(prefs_parse(file, n, handle) == 4 && strcmp(handle, "NEON") == 0);
    CHECK(prefs_server(file, n, url) == 10 && strcmp(url, "TCP://h:1/") == 0);
    n = prefs_format("NEON", "", &def, file);
    CHECK(prefs_server(file, n, url) == 0);                      /* none known: no SERVER line written */
}

static void test_defaults_are_cards_hints_off_sound_on(void)
{
    struct prefs_set s;
    prefs_defaults(&s);
    CHECK(s.view == 0 && s.hints == 0 && s.sound == 1);
    prefs_settings("", 0, &s);                                  /* no file: the same */
    CHECK(s.view == 0 && s.hints == 0 && s.sound == 1);
}
static void test_each_setting_is_read(void)
{
    struct prefs_set s;
    const char f[] = "HANDLE=A\nVIEW=TABLE\nHINTS=ON\nSOUND=OFF\n";
    prefs_defaults(&s);
    prefs_settings(f, sizeof f - 1, &s);
    CHECK(s.view == 1 && s.hints == 1 && s.sound == 0);
}
static void test_keys_and_values_in_any_case(void)
{
    struct prefs_set s;
    const char f[] = "view=table\r\nhints=on\r\nsound=off\r\n";
    prefs_defaults(&s);
    prefs_settings(f, sizeof f - 1, &s);
    CHECK(s.view == 1 && s.hints == 1 && s.sound == 0);
}
static void test_mangled_values_keep_the_default(void)
{
    struct prefs_set s;
    const char f[] = "VIEW=SIDEWAYS\nHINTS=\nSOUND=MAYBE\nVIEW\nHINTS=O\n";
    prefs_defaults(&s);
    prefs_settings(f, sizeof f - 1, &s);
    CHECK(s.view == 0 && s.hints == 0 && s.sound == 1);
}
static void test_settings_roundtrip_through_the_format(void)
{
    struct prefs_set s, r;
    char file[160];
    unsigned char n;
    s.view = 1; s.hints = 1; s.sound = 0;
    n = prefs_format("NEON", "TCP://h:1/", &s, file);
    prefs_defaults(&r);
    prefs_settings(file, n, &r);
    CHECK(r.view == 1 && r.hints == 1 && r.sound == 0);
    s.view = 0; s.hints = 0; s.sound = 1;
    n = prefs_format("NEON", "", &s, file);
    r.view = 1; r.hints = 1; r.sound = 0;
    prefs_settings(file, n, &r);
    CHECK(r.view == 0 && r.hints == 0 && r.sound == 1);
}
static void test_the_longest_file_fits_the_buffer(void)
{
    struct prefs_set s;
    char file[PREFS_FILE_MAX + 1], url[SERVER_MAX + 1];
    unsigned char n;
    memset(url, 'a', SERVER_MAX);
    memcpy(url, "TCP://", 6);
    url[SERVER_MAX] = 0;
    s.view = s.hints = 1; s.sound = 0;
    n = prefs_format("ABCDEFGH", url, &s, file);
    CHECK(n <= PREFS_FILE_MAX);
}

int main(void)
{
    prefs_defaults(&def);
    RUN(test_roundtrip); RUN(test_lowercase_upcased); RUN(test_bad_chars_dropped);
    RUN(test_missing_file_gives_empty); RUN(test_overlong_truncated_to_8); RUN(test_handle_line_found_among_others);
    RUN(test_server_line_found); RUN(test_server_accepts_the_n_prefix_and_lowercase_key); RUN(test_server_must_be_tcp);
    RUN(test_server_too_long_is_ignored); RUN(test_format_keeps_the_server);
    RUN(test_defaults_are_cards_hints_off_sound_on); RUN(test_each_setting_is_read); RUN(test_keys_and_values_in_any_case);
    RUN(test_mangled_values_keep_the_default); RUN(test_settings_roundtrip_through_the_format); RUN(test_the_longest_file_fits_the_buffer);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
