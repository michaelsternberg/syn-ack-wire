/* The platform.h network calls for the Heathkit H89: the H89 FujiNet card (an 82C55 at D0h), through
 * fujinet-lib (lib/fujinet-lib/h89-*, built from source by tools/h89/fnlib.sh). Every call returns 0 when
 * fine, else fujinet-lib's error code or NET_CLOSED. As src/msdos/net_fn.c, which this follows, with the
 * BIOS's clock (h89.h) for the tick. */
#include <fujinet-network.h>
#include <fujinet-fuji.h>
#include "../platform.h"
#include "h89.h"

unsigned char rx_ring[256];
unsigned char rx_head, rx_tail, tx[16];

static const char dev[] = "N:";         /* the unit; the URL is only needed to open */
static uint16_t bw;
static unsigned char conn, err, n;      /* err: FujiNet's own code, 1 = fine */
static unsigned char quiet, asked;      /* nothing came last time; the tick we last asked on */
static unsigned char failing, fail_t;   /* exchanges with FujiNet failing; the tick that began */

#define FAIL_TICKS  186                 /* about 3 seconds of failed exchanges: the card is gone */

/* An exchange with the card that failed is let go: the game asks again next tick. Only when they have
 * failed for FAIL_TICKS does it count as a dropped line. FujiNet answering that the connection closed
 * still counts at once. */
static unsigned char failed(unsigned char code)
{
    if (!failing) {
        failing = 1;
        fail_t = TICK;
    }
    quiet = 1;                          /* next tick */
    return (unsigned char)(TICK - fail_t) >= FAIL_TICKS ? code : 0;
}

/* A failed open may still answer fine: the failure shows in the status after it. */
unsigned char net_open(const char *url)
{
    rx_head = rx_tail = 0;              /* anything left from an old connection is junk */
    quiet = failing = 0;
    if ((n = network_open(url, OPEN_MODE_RW, OPEN_TRANS_NONE)) != 0
        || (n = network_status(dev, &bw, &conn, &err)) != 0)
        return n;
    if (conn)
        return 0;
    return err > 1 ? err : NET_CLOSED;
}

/* One status call and, if bytes are waiting, one read of as many as fit before the ring's end without
 * overrunning unparsed bytes. While nothing comes it asks once a tick; while bytes arrive, every call asks. */
unsigned char net_poll(void)
{
    if (quiet && TICK == asked)
        return 0;
    asked = TICK;
    if ((n = network_status(dev, &bw, &conn, &err)) != 0)
        return failed(n);
    failing = 0;
    if (!bw) {
        quiet = 1;
        return conn ? 0 : NET_CLOSED;
    }
    quiet = 0;                          /* data: ask again at once, till it stops */
    n = bw > 255 ? 255 : bw;
    if (n > (unsigned char)(rx_head - rx_tail - 1))     /* free space */
        n = rx_head - rx_tail - 1;
    if (rx_tail && n > (unsigned char)-rx_tail)         /* room to the ring's end */
        n = -rx_tail;
    if (!n)
        return 0;                       /* ring full: the assembler drains it first */
    if (network_read(dev, rx_ring + rx_tail, n) != n)
        return failed(NET_CLOSED);      /* bytes lost, if any: the stream's resync finds its place */
    rx_tail += n;
    return 0;
}

unsigned char net_send(unsigned char len)
{
    quiet = 0;                          /* the answer is on its way: look at once */
    return network_write(dev, tx, len);
}

void net_close(void)
{
    network_close(dev);
}

/* The lobby's AppKey (creator 1, app 1) as a C string from rx_ring[2]; its length, 0 if none.
 * Counted up to the first NUL, as on the Apple II, in case a missing key reads as zeros. */
unsigned char net_appkey(unsigned char key)
{
    uint16_t len = 0;
    fuji_set_appkey_details(1, 1, DEFAULT);
    if (!fuji_read_appkey(key, &len, rx_ring + 2) || len > 64)
        len = 0;
    rx_ring[2 + len] = 0;
    for (n = 0; rx_ring[2 + n]; ++n)
        ;
    return n;
}
