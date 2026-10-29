/* The platform.h network calls for the NES: the FujiNet NES cartridge's mailbox
 * at $5000-$57FF, through fujinet-lib (lib/fujinet-lib, a pinned copy). Every
 * call returns 0 when fine, else fujinet-lib's error code or NET_CLOSED. As the
 * CoCo's (src/coco/net_fn.c), less its carrier-detect hint.
 *
 * The cartridge has no signal for "data waiting", so net_poll asks for the
 * status about 10 times a second while nothing comes, and at once while bytes
 * arrive or an answer is due. It reads with network_bus_read, for no more than
 * the status just said is waiting: fujinet-lib's network_read asks for the
 * status again first, and waits while nothing is there.
 *
 * The cartridge gives a network call up to 90 s (its firmware, an open's HTTPS
 * round trip), so fujinet-lib's own wait is set past it: one that gave up
 * sooner would leave the reply to be taken for the next call's. Before the
 * first call it waits for the cartridge's link to the FujiNet, up to 15 s, and
 * the first call, which can time out right after power-up, gets a second try. */
#include <fujinet-network.h>
#include <fujinet-fuji.h>
#include <fujinet-bus-nes.h>
#include "../platform.h"

#define POLL_TICKS  6                   /* frames between asks while nothing comes */
#define LINK_WAIT   15                  /* seconds for the cartridge's link, at start */

unsigned char rx_head, rx_tail, tx[16];   /* rx_ring: ring.s, page-aligned */

static const char dev[] = "N:";         /* the unit; the URL is only needed to open */
static unsigned bw;
static unsigned char conn, err, n;      /* err: FujiNet's own code, 1 = fine */
static unsigned char flowing, asked;    /* bytes came last time / when it last asked */
static unsigned char started;

static void start(void)
{
    unsigned char s, t;

    started = 1;
    fn_default_timeout = 95;
    for (s = 0; s < LINK_WAIT && !(FN_STATUS & FN_STATUS_LINK); ++s)
        for (t = ticks(); (unsigned char)(ticks() - t) < ticks_per_sec();)
            if (FN_STATUS & FN_STATUS_LINK)
                break;
}

/* A failed open may still answer fine: the failure shows in the status after
 * it (not connected, FujiNet's error code). */
unsigned char __fastcall__ net_open(const char *url)
{
    if (!started)
        start();
    rx_head = rx_tail = 0;              /* anything left from an old connection is junk */
    flowing = 0;
    if ((n = network_open(url, OPEN_MODE_RW, OPEN_TRANS_NONE)) != 0
        || (n = network_status(dev, &bw, &conn, &err)) != 0)
        return n;
    if (conn)
        return 0;
    return err > 1 ? err : NET_CLOSED;
}

/* One status call and, if bytes are waiting, one read of as many as fit before
 * the ring's page end without overrunning unparsed bytes. */
unsigned char net_poll(void)
{
    if (!flowing && (unsigned char)(ticks() - asked) < POLL_TICKS)
        return 0;
    asked = ticks();
    if ((n = network_status(dev, &bw, &conn, &err)) != 0)
        return n;
    if (!bw) {
        flowing = 0;
        return conn ? 0 : NET_CLOSED;
    }
    flowing = 1;                        /* data: ask again at once, till it stops */
    n = bw > 255 ? 255 : (unsigned char)bw;
    if (n > (unsigned char)(rx_head - rx_tail - 1))     /* free space */
        n = rx_head - rx_tail - 1;
    if (rx_tail && n > (unsigned char)-rx_tail)         /* room to the page end */
        n = -rx_tail;
    if (!n)
        return 0;                       /* ring full: the assembler drains it first */
    if ((n = (unsigned char)network_bus_read(FUJI_DEVICEID_NETWORK, rx_ring + rx_tail, n)) == 0)
        return NET_CLOSED;
    rx_tail += n;
    return 0;
}

unsigned char __fastcall__ net_send(unsigned char len)
{
    flowing = 1;                        /* the answer is on its way: look at once */
    return network_write(dev, tx, len);
}

void net_close(void)
{
    network_close(dev);
}

/* The lobby's AppKey (creator 1, app 1) as a C string from rx_ring[2]; its length, 0 if none.
 * The length is counted up to the first NUL, as on the Apple, in case a missing key reads
 * as a success. The very first call gets a second try. */
unsigned char __fastcall__ net_appkey(unsigned char key)
{
    unsigned len = 0;
    unsigned char ok;

    if (!started)
        start();
    fuji_set_appkey_details(1, 1, DEFAULT);
    ok = fuji_read_appkey(key, &len, rx_ring + 2);
    if (!ok && started == 1)
        ok = fuji_read_appkey(key, &len, rx_ring + 2);
    started = 2;
    if (!ok || len > 64)
        len = 0;
    rx_ring[2 + len] = 0;
    for (n = 0; rx_ring[2 + n]; ++n)
        ;
    return n;
}
