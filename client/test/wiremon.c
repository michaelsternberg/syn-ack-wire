/* wiremon: watch the SYN-ACK WIRE protocol on an Atari.
 *
 * Connects through FujiNet, says hello, and prints every frame from the server:
 * its type letter and payload in hex, or the text of log lines. It proves the
 * network layer (open, status, read, write, ring wrap, reconnect) before any
 * game screens exist.
 *
 * Keys: 2-6 start a game with that many seats (host only), R resync, Q leave.
 * At connect it asks for RESYNCS full resyncs to push a few KB through the ring.
 * Built with -DAUTOTEST it then starts a 3-seat game itself and sends Q once the
 * game is over (then stops), or as soon as a bad frame arrives, so a test
 * script can read the result from the server log.
 *
 * On MS-DOS (built with Open Watcom, __DOS__) the 40x24 screen is a buffer in
 * memory, shown in the middle of the 80-column text screen. The AUTOTEST build
 * also writes it to WIREMON.TXT every few seconds and when it stops, for a test
 * script to read from the disk.
 *
 * On the Heathkit H89 (z88dk, __H89__) the same buffer goes to the H19 terminal's columns 20-59, only the
 * cells that changed since the last time, each run of them after a cursor move (ESC Y).
 */
#include <string.h>
#ifdef __DOS__
#include <stdio.h>
#include <bios.h>
#elif defined(__H89__)
#include "../src/h89/h89.h"
#else
#include <atari.h>
#endif
#include "../src/platform.h"
#include "../src/common/proto.h"
#include "../src/common/proto_tab.h"

#ifndef URL
#define URL "N:TCP://localhost:6502/"
#endif
#define TABLE   0
#define RESYNCS 10

#ifdef __DOS__
static unsigned char screen[24 * 40];
#define RTCLOK (*(volatile unsigned char __far *)0x0040006CL)  /* BIOS tick, 18.2 a second */
#define SECOND 18
#define CHAR(c) (c)                     /* the buffer holds ASCII; 0 shows as blank */
#elif defined(__H89__)
static unsigned char screen[24 * 40], shown[24 * 40];
#define RTCLOK TICK
#define SECOND TICKS_PER_SEC
#define CHAR(c) (c)
#else
#define SAVMSC (*(unsigned char **)0x58)
#define RTCLOK (*(volatile unsigned char *)0x14)
#define CH     (*(volatile unsigned char *)0x2FC)
#define SECOND 60
#define CHAR(c) ((c) - 0x20)            /* ATASCII $20-$5F is internal $00-$3F */
#endif

static unsigned char *scr, *cur;
static unsigned char token[4];
static unsigned frames, bytes;
static unsigned char bad, resyncs, greeted, t, j, k, e;
#ifdef AUTOTEST
static unsigned char started;           /* 1: S sent, 2: game running, 3: done */
#ifdef __DOS__
static unsigned char passes;            /* main loop passes, for the periodic save */
#endif
#endif

/* ---------------------------------------------------------------- screen
 * The Atari's screen memory takes internal codes: see CHAR. */

static void put(unsigned char c)
{
    *cur++ = CHAR(c);
}

static void puts_(const char *s)
{
    while (*s)
        put(*s++);
}

static void hex(unsigned char b)
{
    put("0123456789ABCDEF"[b >> 4]);
    put("0123456789ABCDEF"[b & 15]);
}

static void dec(unsigned n)
{
    static const unsigned pow[] = { 10000, 1000, 100, 10, 1 };
    unsigned char d;
    for (j = 0; j < 5; ++j) {
        for (d = '0'; n >= pow[j]; ++d)
            n -= pow[j];
        put(d);
    }
}

/* Scroll rows 3-23 up one and point cur at a blank row 23. */
static void newline(void)
{
    cur = scr + 23 * 40;
    memmove(scr + 3 * 40, scr + 4 * 40, 20 * 40);
    memset(cur, 0, 40);
}

#ifdef __H89__
static void show_screen(void);
#endif

static void say(const char *s)
{
    newline();
    puts_(s);
#ifdef __H89__
    show_screen();                      /* now: a call to FujiNet that hangs shows what it was */
#endif
}

#ifdef __DOS__
/* Copy the buffer to the middle of the 80x25 text screen: B000 on a monochrome
 * adapter (BIOS video mode 7), else B800. Normal attribute. */
static void show_screen(void)
{
    unsigned short __far *v = *(volatile unsigned char __far *)0x00400049L == 7
        ? (unsigned short __far *)0xB0000000L : (unsigned short __far *)0xB8000000L;
    unsigned i;
    for (i = 0; i < 24 * 40; ++i)
        v[i / 40 * 80 + 20 + i % 40] = 0x0700 | screen[i];
}

#ifdef AUTOTEST
/* The buffer as text, for a test script: WIREMON.TXT on the current drive. */
static void save_screen(void)
{
    FILE *f = fopen("WIREMON.TXT", "w");
    unsigned i;
    if (!f)
        return;
    for (i = 0; i < 24 * 40; ++i) {
        fputc(screen[i] ? screen[i] : ' ', f);
        if (i % 40 == 39)
            fputc('\n', f);
    }
    fclose(f);
}
#endif
#elif defined(__H89__)
/* Send the H19 the cells that changed: a cursor move, then the run. BDOS 6 writes a byte as it is. */
static void show_screen(void)
{
    unsigned i = 0;
    unsigned char c;
    while (i < 24 * 40) {
        if (screen[i] == shown[i]) {
            ++i;
            continue;
        }
        bdos(6, 0x1B);
        bdos(6, 'Y');
        bdos(6, 32 + i / 40);
        bdos(6, 32 + 20 + i % 40);
        do {
            c = screen[i];
            shown[i] = c;
            bdos(6, c ? c : ' ');
        } while (++i % 40 && screen[i] != shown[i]);
    }
}
#endif

static void status(void)
{
    cur = scr + 40;
    puts_("FRAMES ");
    dec(frames);
    puts_("  BYTES ");
    dec(bytes);
    puts_("  BAD ");
    hex(bad);
#if defined(__DOS__) || defined(__H89__)
    show_screen();
#endif
}

/* ---------------------------------------------------------------- network */

static void send(unsigned char len)
{
    if ((e = net_send(len)) != 0) {
        say("SEND FAILED ");
        hex(e);
    }
}

static void send1(unsigned char type, unsigned char arg, unsigned char len)
{
    tx[0] = type;
    tx[1] = arg;
    send(len);
}

static void hello(void)
{
    tx[0] = 'H';
    tx[1] = PROTO_VERSION;
    tx[2] = TABLE;
    for (j = 0; j < 4; ++j)
        tx[3 + j] = token[j];
    for (j = 0; j < 8; ++j)
        tx[7 + j] = "WIREMON "[j];
    send(1 + CLEN_H);
}

static void wait(unsigned char jiffies)
{
    j = RTCLOK;
    while ((unsigned char)(RTCLOK - j) < jiffies)
        ;
}

static void connect(void)
{
    for (;;) {
        say("OPEN " URL);
        if ((e = net_open(URL)) == 0)
            break;
        say("OPEN FAILED ");
        hex(e);
#ifdef __H89__
        show_screen();
#endif
        net_close();
        wait(2 * SECOND);
    }
    hello();
}

/* Print one frame: type, then log text or payload bytes in hex. */
static void show(void)
{
    newline();
    put(t);
    put(' ');
    if (t == 'l') {
        for (j = 0; j < msg[1] && j < 38; ++j)
            put(msg[2 + j]);
        return;
    }
    k = t == 'a' || t == 'p' || t == 'x' ? msg[1] + 1 : len_tab[t - 'a'];
    for (j = 1; j <= k && j <= 12; ++j) {
        hex(msg[j]);
        put(' ');
    }
    if (k > 12)
        puts_("+");
}

static void frame(void)
{
    ++frames;
    bytes += t == 'a' || t == 'l' || t == 'p' || t == 'x' ? msg[1] + 2 : len_tab[t - 'a'] + 1;
    show();
    switch (t) {
    case 'w':                           /* welcome: keep the seat token */
        for (j = 0; j < 4; ++j)
            token[j] = msg[2 + j];
        if (!greeted) {                 /* every resync starts with w: count once */
            greeted = 1;
            resyncs = RESYNCS;
        }
        break;
    case 'f':
        if (msg[1] == F_TOKEN)          /* server forgot us: come back as new */
            for (j = 0; j < 4; ++j)
                token[j] = 0;
        break;
    case 's':                           /* a resync finished: ask for the next */
        if (!resyncs)
            break;
        if (--resyncs) {
            send1('R', 0, 1);
            break;
        }
#ifdef AUTOTEST
        if (!bad) {
            started = 1;
            send1('S', 3, 2);
        }
#endif
        break;
#ifdef AUTOTEST
    case 't':                           /* a lobby resync can carry the last */
        if (started == 1)               /* game's e, so only count e after t */
            started = 2;
        break;
    case 'e':
        if (started == 2) {
            started = 3;
            say("AUTOTEST DONE");
            send1('Q', 0, 1);
#ifdef __DOS__
            save_screen();
#endif
        }
        break;
#endif
    }
}

#ifdef __DOS__
static void keys(void)
{
    if (!_bios_keybrd(_KEYBRD_READY))
        return;
    switch ((unsigned char)_bios_keybrd(_KEYBRD_READ) & 0xDF) {     /* ASCII, upper case */
    case '2' & 0xDF: send1('S', 2, 2); break;
    case '3' & 0xDF: send1('S', 3, 2); break;
    case '4' & 0xDF: send1('S', 4, 2); break;
    case '5' & 0xDF: send1('S', 5, 2); break;
    case '6' & 0xDF: send1('S', 6, 2); break;
    case 'R': send1('R', 0, 1); break;
    case 'Q': send1('Q', 0, 1); break;
    }
}
#elif defined(__H89__)
static void keys(void)
{
    switch (bdos(6, 0xFF) & 0xDF) {     /* a key or 0, upper case */
    case '2' & 0xDF: send1('S', 2, 2); break;
    case '3' & 0xDF: send1('S', 3, 2); break;
    case '4' & 0xDF: send1('S', 4, 2); break;
    case '5' & 0xDF: send1('S', 5, 2); break;
    case '6' & 0xDF: send1('S', 6, 2); break;
    case 'R': send1('R', 0, 1); break;
    case 'Q': send1('Q', 0, 1); break;
    }
}
#else
static void keys(void)
{
    k = CH;
    if (k == 0xFF)
        return;
    CH = 0xFF;
    switch (k) {
    case 0x1E: send1('S', 2, 2); break; /* 2 */
    case 0x1A: send1('S', 3, 2); break; /* 3 */
    case 0x18: send1('S', 4, 2); break; /* 4 */
    case 0x1D: send1('S', 5, 2); break; /* 5 */
    case 0x1B: send1('S', 6, 2); break; /* 6 */
    case 0x28: send1('R', 0, 1); break; /* R */
    case 0x2F: send1('Q', 0, 1); break; /* Q */
    }
}
#endif

void main(void)
{
#if defined(__DOS__) || defined(__H89__)
    scr = screen;
#else
    scr = SAVMSC;
#endif
#ifdef __H89__
    bdos(6, 0x1B);                      /* clear the H19 */
    bdos(6, 'E');
    memset(shown, 0, sizeof shown);
#endif
    for (cur = scr; cur < scr + 24 * 40; ++cur)
        *cur = 0;
    cur = scr;
    puts_("WIREMON  2-6 START  R RESYNC  Q LEAVE");
    status();
    connect();
    for (;;) {
        wait(4);                        /* poll at 15 Hz */
        if ((e = net_poll()) != 0) {
#ifdef AUTOTEST
            if (started == 3)
                for (;;)
                    ;
#endif
            say("CARRIER LOST ");
            hex(e);
            net_close();
            wait(2 * SECOND);
            connect();
            continue;
        }
        while ((t = proto_next()) != 0) {
            if (t == PROTO_BAD) {
                ++bad;
                say("BAD FRAME: RESYNC");
                send1('R', 0, 1);
#ifdef AUTOTEST
                send1('Q', 0, 1);
#endif
                break;
            }
            frame();
        }
        status();
        keys();
#if defined(__DOS__) && defined(AUTOTEST)
        if (++passes == 0)              /* every 256 passes: about 14 s */
            save_screen();
#endif
    }
}
