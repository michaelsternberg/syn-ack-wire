/* Host tests for src/amiga/sndlogic.c: which chord wins, the on/off switch, and the notes' periods.
 *   make test-amiga-sound */
#include <stdio.h>
#include "../src/platform.h"
#include "../src/amiga/sndlogic.h"

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); t(); } while (0)

static void test_higher_cuts_lower(void)
{
    CHECK(amiga_chord_wins(CHORD_SPIKE, CHORD_ICE) == 1);
    CHECK(amiga_chord_wins(CHORD_BUY, CHORD_END) == 1);
}
static void test_equal_or_lower_ignored(void)
{
    CHECK(amiga_chord_wins(CHORD_HIVE, CHORD_HIVE) == 0);
    CHECK(amiga_chord_wins(CHORD_END, CHORD_SPIKE) == 0);
}
static void test_nothing_playing_always_starts(void)
{
    CHECK(amiga_chord_wins(SND_NONE, CHORD_SPIKE) == 1);
}
static void test_toggle_on_plays_hive(void)
{
    struct snd_state s = { 0, SND_NONE, 0 };
    CHECK(amiga_snd_toggle(&s, 100, 50) == CHORD_HIVE);     /* off to on: the hive chord, to show it's on */
    CHECK(s.on == 1 && s.playing == CHORD_HIVE);
    CHECK(amiga_snd_toggle(&s, 100, 50) == SND_NONE);       /* on to off: silent */
    CHECK(s.on == 0);
    CHECK(amiga_snd_request(&s, CHORD_END, 100, 50) == SND_NONE);   /* nothing while off */
}
static void test_a_finished_chord_gives_way(void)
{
    struct snd_state s = { 1, SND_NONE, 0 };
    CHECK(amiga_snd_request(&s, CHORD_END, 10, 50) == CHORD_END);
    CHECK(amiga_snd_request(&s, CHORD_SPIKE, 11, 50) == SND_NONE);  /* END still sounding */
    CHECK(amiga_snd_request(&s, CHORD_SPIKE, 10 + 40, 50) == CHORD_SPIKE);
}
static void test_tick_wraparound(void)
{
    struct snd_state s = { 1, SND_NONE, 0 };
    CHECK(amiga_snd_request(&s, CHORD_END, 250, 50) == CHORD_END);  /* ends past 255 */
    CHECK(amiga_snd_request(&s, CHORD_SPIKE, 253, 50) == SND_NONE);
    CHECK(amiga_snd_request(&s, CHORD_SPIKE, 20, 50) == CHORD_SPIKE);
}
static void test_every_note_is_playable(void)
{
    unsigned char c, n;
    for (c = 0; c < 7; ++c)
        for (n = 0; n < 3; ++n) {
            unsigned short hz = amiga_chord_notes[c][n][0];
            if (!hz) continue;
            CHECK(amiga_note_period(hz, 3546895UL) >= 124);     /* Paula's shortest period */
            CHECK(amiga_note_cycles(hz, amiga_chord_notes[c][n][1]) >= 1);
        }
    CHECK(amiga_note_period(459, 3546895UL) == 966);
}

int main(void)
{
    RUN(test_higher_cuts_lower); RUN(test_equal_or_lower_ignored); RUN(test_nothing_playing_always_starts);
    RUN(test_toggle_on_plays_hive); RUN(test_a_finished_chord_gives_way); RUN(test_tick_wraparound);
    RUN(test_every_note_is_playable);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
