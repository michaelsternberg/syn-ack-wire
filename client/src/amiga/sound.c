/* Chords on Paula through audio.device. One request per note, queued behind the one before, so nothing blocks;
 * a chord that is cut off has its queue aborted. If audio.device is not there, or busy with another program,
 * the game is silent and plays on. */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "../platform.h"
#include "sndlogic.h"

#define NOTES    3
#define CYCLE    8                      /* bytes in one wave: a square */
#define PAL_CLOCK  3546895UL
#define NTSC_CLOCK 3579545UL

static struct snd_state st = { 1, SND_NONE, 0 };
static struct MsgPort *port;
static struct IOAudio *req[NOTES];
static BYTE *wave;                      /* chip RAM */
static unsigned char open, queued, pending;
static unsigned long clock;

void snd_init(void)
{
    static UBYTE want[4] = { 1, 2, 4, 8 };       /* any one channel: the notes follow each other, and a write wants one */
    unsigned char i;

    clock = SysBase->VBlankFrequency == 50 ? PAL_CLOCK : NTSC_CLOCK;
    wave = AllocMem(CYCLE, MEMF_CHIP);
    port = CreatePort(0, 0);
    if (!wave || !port)
        return;
    for (i = 0; i < CYCLE; ++i)
        wave[i] = i < CYCLE / 2 ? 50 : -50;
    for (i = 0; i < NOTES; ++i) {
        req[i] = (struct IOAudio *)CreateExtIO(port, sizeof(struct IOAudio));
        if (!req[i])
            return;
    }
    req[0]->ioa_Request.io_Message.mn_Node.ln_Pri = 0;
    req[0]->ioa_Data = want;
    req[0]->ioa_Length = sizeof want;
    req[0]->ioa_Request.io_Command = ADCMD_ALLOCATE;
    req[0]->ioa_AllocKey = 0;
    if (OpenDevice((UBYTE *)"audio.device", 0, (struct IORequest *)req[0], 0) != 0)
        return;
    open = 1;
    for (i = 1; i < NOTES; ++i) {       /* the others share the first's channels and key */
        req[i]->ioa_Request.io_Device = req[0]->ioa_Request.io_Device;
        req[i]->ioa_Request.io_Unit = req[0]->ioa_Request.io_Unit;
        req[i]->ioa_AllocKey = req[0]->ioa_AllocKey;
    }
}

static void reap(void)
{
    struct Message *m;
    while (pending && (m = GetMsg(port)) != 0)
        --pending;
}

static void cut(void)
{
    unsigned char i;
    for (i = 0; i < queued; ++i)
        AbortIO((struct IORequest *)req[i]);
    for (; pending; --pending)
        WaitPort(port), GetMsg(port);
    queued = 0;
}

static void start(unsigned char n)
{
    unsigned char k, count = 0;

    reap();
    if (pending)
        cut();
    for (k = 0; k < NOTES && amiga_chord_notes[n][k][0]; ++k) {
        struct IOAudio *r = req[k];
        unsigned short hz = amiga_chord_notes[n][k][0];

        r->ioa_Request.io_Command = CMD_WRITE;
        r->ioa_Request.io_Flags = ADIOF_PERVOL;
        r->ioa_Data = (UBYTE *)wave;
        r->ioa_Length = CYCLE;
        r->ioa_Period = amiga_note_period(hz, clock);
        r->ioa_Volume = 48;
        r->ioa_Cycles = amiga_note_cycles(hz, amiga_chord_notes[n][k][1]);
        BeginIO((struct IORequest *)r);
        ++count;
    }
    queued = pending = count;
}

void __fastcall__ play_chord(unsigned char n)
{
    if (open && amiga_snd_request(&st, n, ticks(), ticks_per_sec()) != SND_NONE)
        start(n);
}

void amiga_snd_set(unsigned char on)
{
    if (st.on != on)
        snd_toggle();
}

unsigned char amiga_snd_on(void)
{
    return st.on;
}

void snd_toggle(void)
{
    unsigned char c = amiga_snd_toggle(&st, ticks(), ticks_per_sec());

    if (!open)
        return;
    if (!st.on)
        cut();
    else if (c != SND_NONE)
        start(c);
}

/* Give Paula back: the channels stay taken until the device is closed. */
__attribute__((destructor)) static void snd_exit(void)
{
    unsigned char i;

    if (open) {
        cut();
        CloseDevice((struct IORequest *)req[0]);
        open = 0;
    }
    for (i = 0; i < NOTES; ++i)
        if (req[i])
            DeleteExtIO((struct IORequest *)req[i]), req[i] = 0;
    if (port)
        DeletePort(port), port = 0;
    if (wave)
        FreeMem(wave, CYCLE), wave = 0;
}
