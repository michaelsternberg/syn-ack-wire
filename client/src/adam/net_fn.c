/* The platform.h network calls for the Adam: FujiNet's network device on
 * AdamNet (N1:, device 09h). The open, the close and the AppKeys are
 * fujinet-lib's (lib/fujinet-lib, a pinned copy); the stream between them is
 * the device's DCB driven by hand and never waited on, as
 * fujinet-multiplayer-mule's src/adam/nstrans.c does it. Every call returns 0
 * when fine, else fujinet-lib's error code or NET_CLOSED.
 *
 * fujinet-lib's calls set a DCB going and spin until it finishes, retrying a
 * timeout 20 times. An AdamNet exchange can take a frame or more (FujiNet Go
 * Adam answers about once a frame), and a READ the FujiNet leaves unanswered,
 * as it does one with nothing waiting, is a timeout. Here net_poll starts a
 * call and looks at it again next time, so the game goes on drawing and
 * reading keys whatever the bus does. Each look takes what finished and
 * starts the next call: a WRITE of what net_send queued (it goes first), else
 * an AdamNet STATUS, whose device status byte (DCB+20) says whether bytes wait
 * (bit 0) and the socket is up (bit 1), every frame while bytes come and
 * about 10 times a second while nothing does; and a READ only when bytes wait.
 *
 * A READ brings one packet, whatever length it asks for: as many bytes as
 * wait, up to 1024 (FujiNet's packet), so it reads into a buffer that size,
 * and net_poll hands them to rx_ring as the frame assembler makes room. The
 * link is down after DOWN_AFTER statuses in a row say the socket is shut. */
#include <string.h>
#include <fujinet-network.h>
#include <fujinet-fuji.h>
#include <fujinet-commands.h>
#include "../platform.h"

/* the DCB (EOS keeps FEC3h of them at FEC4h, 21 bytes each) */
#define D_STATUS    0                   /* the command; 80h and up when done */
#define D_BUF       1
#define D_LEN       3                   /* the length; a read's is put back */
#define D_DEV       16
#define D_DEVSTAT   20                  /* the device's status byte */

#define C_STATUS    1
#define C_WRITE     3
#define C_READ      4
#define S_OK        0x80

#define NET_DEV     0x09                /* N1: */
#define DS_DATA     0x01                /* bytes wait */
#define DS_CONN     0x02                /* the socket is up */
#define DOWN_AFTER  3                   /* statuses in a row saying the socket is shut */
#define WRITE_TRIES 10
#define POLL_TICKS  6                   /* frames between statuses while nothing comes */
#define QUIET_TICKS 240                 /* the longest a call can be waited for */

enum { T_IDLE, T_STATUS, T_READ, T_WRITE };

unsigned char rx_ring[256], rx_head, rx_tail, tx[16];

static const char dev[] = "N:";         /* the unit; the URL is only needed to open */
static volatile unsigned char *dcb;
static unsigned char state, down, wtries, flowing, asked, n;
static unsigned char rbuf[1024];
static unsigned rlen, rpos;
static unsigned char wbuf[1 + 2 * sizeof tx], wlen;     /* 'W', then the frames */
static unsigned char qbuf[2 * sizeof tx], qlen;         /* sent, not yet written */

static void start(unsigned char cmd, void *buf, unsigned len)
{
    dcb[D_BUF] = (unsigned char)(unsigned)buf;
    dcb[D_BUF + 1] = (unsigned)buf >> 8;
    dcb[D_LEN] = (unsigned char)len;
    dcb[D_LEN + 1] = len >> 8;
    dcb[D_STATUS] = cmd;                /* (last: this sets it going) */
}

/* What's going, finished (bounded: an unanswered call ends in a timeout). */
static void quiesce(void)
{
    unsigned char t, s = 0;

    if (!dcb || state == T_IDLE)
        return;
    for (t = ticks(); dcb[D_STATUS] < 0x80 && s < QUIET_TICKS / 60; )
        if ((unsigned char)(ticks() - t) >= 60) {
            t += 60;
            ++s;
        }
    state = T_IDLE;
}

static void find_dcb(void)
{
    unsigned char k = *(unsigned char *)0xFEC3;
    volatile unsigned char *d = (volatile unsigned char *)0xFEC4;

    for (dcb = 0; k; --k, d += 21)
        if ((d[D_DEV] & 0x0F) == NET_DEV) {
            dcb = d;
            return;
        }
}

/* The connection is made here, a wait, once. A failed open may still answer
 * fine: the failure shows in the status after it (not connected, FujiNet's
 * error code). */
unsigned char __fastcall__ net_open(const char *url)
{
    unsigned bw;
    unsigned char conn, err;

    quiesce();
    find_dcb();
    if (!dcb)
        return NET_CLOSED;
    rx_head = rx_tail = 0;              /* anything left from an old connection is junk */
    rlen = rpos = 0;
    qlen = wlen = down = flowing = 0;
    if ((n = network_open(url, OPEN_MODE_RW, OPEN_TRANS_NONE)) != 0
        || (n = network_status(dev, &bw, &conn, &err)) != 0)
        return n;
    if (conn)
        return 0;
    return err > 1 ? err : NET_CLOSED;
}

void net_close(void)
{
    quiesce();
    qlen = wlen = 0;
    if (dcb)
        network_close(dev);
}

static void start_write(void)
{
    start(C_WRITE, wbuf, wlen);
    state = T_WRITE;
}

/* One look at the DCB: what finished is taken, and the next call started. */
static void step(void)
{
    unsigned char s;

    if (state != T_IDLE) {
        s = dcb[D_STATUS];
        if (s < 0x80)
            return;                     /* still going */
        switch (state) {
        case T_STATUS:
            state = T_IDLE;
            if (s != S_OK)
                break;
            s = dcb[D_DEVSTAT];
            if (s & DS_CONN)
                down = 0;
            else if (down < 255)
                ++down;
            if (s & DS_DATA) {
                flowing = 1;
                start(C_READ, rbuf, sizeof rbuf);
                state = T_READ;
                return;
            }
            flowing = 0;
            break;
        case T_READ:
            state = T_IDLE;
            if (s == S_OK) {
                rlen = dcb[D_LEN] | (unsigned)dcb[D_LEN + 1] << 8;
                if (rlen > sizeof rbuf)
                    rlen = sizeof rbuf;
                rpos = 0;
            }
            return;                     /* (the bytes first) */
        case T_WRITE:
            state = T_IDLE;
            if (s != S_OK && ++wtries < WRITE_TRIES) {
                start_write();          /* the same again */
                return;
            }
            wlen = 0;
            break;
        }
    }
    if (qlen) {
        wbuf[0] = NETCMD_WRITE;
        memcpy(wbuf + 1, qbuf, qlen);
        wlen = qlen + 1;
        qlen = 0;
        wtries = 0;
        start_write();
        return;
    }
    if (rpos < rlen)
        return;                         /* bytes still to hand over: no read yet */
    if (flowing || (unsigned char)(ticks() - asked) >= POLL_TICKS) {
        asked = ticks();
        start(C_STATUS, 0, 0);
        state = T_STATUS;
    }
}

/* One look at the DCB, then as many bytes as fit into the ring without
 * overrunning unparsed ones. */
unsigned char net_poll(void)
{
    if (!dcb)
        return NET_CLOSED;
    step();
    if (down >= DOWN_AFTER)
        return NET_CLOSED;
    while (rpos < rlen && (unsigned char)(rx_tail + 1) != rx_head)
        rx_ring[rx_tail++] = rbuf[rpos++];
    return 0;
}

/* Queued; the next look writes it. */
unsigned char __fastcall__ net_send(unsigned char len)
{
    if (!dcb || qlen + len > sizeof qbuf)
        return NET_CLOSED;
    memcpy(qbuf + qlen, tx, len);
    qlen += len;
    step();
    return 0;
}

/* The lobby's AppKey (creator 1, app 1) as a C string from rx_ring[2]; its length, 0 if none.
 * The length is counted up to the first NUL, as on the Apple, in case a missing key reads
 * as a success. */
unsigned char __fastcall__ net_appkey(unsigned char key)
{
    unsigned len = 0;

    quiesce();
    fuji_set_appkey_details(1, 1, DEFAULT);
    if (!fuji_read_appkey(key, &len, rx_ring + 2) || len > 64)
        len = 0;
    rx_ring[2 + len] = 0;
    for (n = 0; rx_ring[2 + n]; ++n)
        ;
    return n;
}
