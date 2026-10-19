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
 */
#include <atari.h>
#include <string.h>
#include "../src/platform.h"
#include "../src/common/proto.h"
#include "../src/common/proto_tab.h"

#ifndef URL
#define URL "N:TCP://localhost:6502/"
#endif
#define TABLE   0
#define RESYNCS 10

#define SAVMSC (*(unsigned char **)0x58)
#define RTCLOK (*(volatile unsigned char *)0x14)
#define CH     (*(volatile unsigned char *)0x2FC)

static unsigned char *scr, *cur;
static unsigned char token[4];
static unsigned frames, bytes;
static unsigned char bad, resyncs, greeted, t, j, k, e;
#ifdef AUTOTEST
static unsigned char started;           /* 1: S sent, 2: game running, 3: done */
#endif

/* ---------------------------------------------------------------- screen
 * Screen memory takes internal codes: ATASCII $20-$5F is internal $00-$3F. */

static void put(unsigned char c)
{
    *cur++ = c - 0x20;
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

static void say(const char *s)
{
    newline();
    puts_(s);
}

static void status(void)
{
    cur = scr + 40;
    puts_("FRAMES ");
    dec(frames);
    puts_("  BYTES ");
    dec(bytes);
    puts_("  BAD ");
    hex(bad);
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
        net_close();
        wait(120);
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
        }
        break;
#endif
    }
}

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

void main(void)
{
    scr = SAVMSC;
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
            wait(120);
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
    }
}
