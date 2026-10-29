/* Chords for board events: the MS-DOS client's AdLib chords
 * (src/msdos/sound.c, after the Atari's), three voices at once, made here
 * with two-operator FM much as the OPL2 makes them: a struck sound whose
 * brightness and loudness die away, released when the chord's time is up and
 * followed by the chord that comes after it, if any. A sounding chord gives way
 * only to a higher-numbered one. */
#include <math.h>
#include <SDL3/SDL.h>
#include "../../client/src/platform.h"
#include "desk.h"

#define RATE    48000
#define NONE    0xFF

/* OPL2 notes, block << 10 | F-number, as in src/msdos/sound.c */
#define D3 0x0B06
#define E3 0x0B65
#define F3 0x0B99
#define G3 0x0E05
#define A3 0x0E44
#define B3 0x0E8B
#define C4 0x0EB2
#define D4 0x0F06
#define E4 0x0F65
#define F4 0x0F99
#define G4 0x1205
#define A4 0x1244

static const unsigned short fm_chord[11][3] = {
    { D3, F3, A3 }, { C4, E4, G4 }, { C4, E4, G4 }, { B3, D4, F4 },   /* spike, buy, hive, hivejack */
    { E3, G3, B3 }, { G3, B3, D4 }, { F3, A3, C4 },                   /* Black ICE, turn, null-route */
    { C4, F4, A4 }, { C4, E4, G4 }, { C4, E4, G4 }, { B3, F4, A4 }    /* what follows buy, turn, null-route */
};
static const unsigned char fm_ticks[11] = { 4, 5, 12, 12, 18, 5, 9, 5, 5, 9, 46 };     /* BIOS ticks, 18.2 a second */
static const unsigned char fm_next[11] = { NONE, 7, NONE, NONE, NONE, 8, 9, NONE, NONE, 10, NONE };

static SDL_AudioStream *stream;
static int on = 1;
/* the audio thread's state, under the stream's lock */
static unsigned char sounding = NONE, prio;
static long held, at;                   /* the chord's length while keyed, and the samples since it began */
static double phase[3], mphase[3], freq[3];
static float release_from;              /* the level when it was let go */

static double hz(unsigned short note)
{
    return (note & 0x3FF) * 49716.0 / (double)(1L << (20 - (note >> 10)));
}

static void start(unsigned char c)
{
    unsigned char v;
    for (v = 0; v < 3; ++v)
        freq[v] = hz(fm_chord[c][v]);
    sounding = c;
    held = (long)(fm_ticks[c] * RATE / 18.2);
    at = 0;
}

static void SDLCALL fill(void *user, SDL_AudioStream *s, int need, int total)
{
    float buf[512];
    (void)user;
    (void)total;
    need /= sizeof(float);
    while (need > 0) {
        int k, n = need < 512 ? need : 512;
        for (k = 0; k < n; ++k) {
            float out = 0;
            if (sounding != NONE) {
                double t = (double)at / RATE;
                float level = (float)exp(-t / 0.45);                    /* decay while keyed */
                double index = 1.6 * exp(-t / 0.12);                    /* the brightness of the strike */
                unsigned char v;
                if (at >= held) {                                       /* keyed off: the release */
                    if (at == held)
                        release_from = level;
                    level = release_from * (float)exp(-(double)(at - held) / RATE / 0.05);
                }
                for (v = 0; v < 3; ++v) {
                    mphase[v] += freq[v] / RATE;
                    phase[v] += freq[v] / RATE;
                    out += (float)sin(2 * SDL_PI_D * phase[v] + index * sin(2 * SDL_PI_D * mphase[v]));
                }
                out *= 0.12f * level * (at < 96 ? at / 96.0f : 1.0f);  /* 2 ms attack: no click */
                if (++at >= held && fm_next[sounding] != NONE)
                    start(fm_next[sounding]);
                else if (at > held + RATE / 4)
                    sounding = NONE;
            }
            buf[k] = out;
        }
        SDL_PutAudioStreamData(s, buf, n * (int)sizeof(float));
        need -= n;
    }
}

void snd_init(void)
{
    SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, RATE };
    if (stream || !SDL_InitSubSystem(SDL_INIT_AUDIO))
        return;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, fill, 0);
    if (stream)
        SDL_ResumeAudioStreamDevice(stream);
}

void play_chord(unsigned char n)
{
    if (!on || !stream)
        return;
    SDL_LockAudioStream(stream);
    if (sounding == NONE || n >= prio) {
        prio = n;
        start(n);
    }
    SDL_UnlockAudioStream(stream);
}

void snd_toggle(void)
{
    snd_set(!on);
    if (on)
        play_chord(CHORD_HIVE);
}

int snd_is_on(void)
{
    return on;
}

void snd_set(int o)
{
    on = o != 0;
    if (!on && stream) {
        SDL_LockAudioStream(stream);
        sounding = NONE;
        SDL_UnlockAudioStream(stream);
    }
}
