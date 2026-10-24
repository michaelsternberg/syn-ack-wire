/* The platform.h network calls for the Apple II: FujiNet over SmartPort,
 * through fujinet-lib (lib/fujinet-lib, a pinned copy). It finds the FujiNet
 * devices on first use. Every call returns 0 when fine, else fujinet-lib's
 * error code or NET_CLOSED. */
#include <fujinet-network.h>
#include <fujinet-fuji.h>
#include "../platform.h"

unsigned char rx_head, rx_tail, tx[16];

static const char dev[] = "N:";         /* the unit; the URL is only needed to open */
static unsigned bw;
static unsigned char conn, err, n;      /* err: FujiNet's own code, 1 = fine */
static unsigned char skip;              /* net_poll calls left before it asks again */
void __fastcall__ pause(unsigned char n);   /* sound.s */

/* Over SmartPort a failed open still answers fine: the failure shows in the
 * status after it (not connected, FujiNet's error code). */
unsigned char __fastcall__ net_open(const char *url)
{
    rx_head = rx_tail = 0;              /* anything left from an old connection is junk */
    skip = 0;
    if ((n = network_open(url, OPEN_MODE_RW, OPEN_TRANS_NONE)) != 0
        || (n = network_status(dev, &bw, &conn, &err)) != 0)
        return n;
    if (conn)
        return 0;
    return err > 1 ? err : NET_CLOSED;
}

/* One status call and, if bytes are waiting, one read of as many as fit before
 * the ring's page end without overrunning unparsed bytes. SmartPort has no
 * signal for "data waiting" (the Atari's PROCEED), so this asks: while nothing
 * comes, only one call in POLL_EVERY does, at most about 10 times a second, the
 * calls between waiting a moment (pause); while bytes arrive, every call does. */
#define POLL_EVERY 40                   /* x 2 ms: a tenth of a second, at least */

unsigned char net_poll(void)
{
    if (skip) {
        --skip;
        pause(8);
        return 0;
    }
    skip = POLL_EVERY;
    if ((n = network_status(dev, &bw, &conn, &err)) != 0)
        return n;
    if (!bw)
        return conn ? 0 : NET_CLOSED;
    skip = 0;                           /* data: ask again at once, till it stops */
    n = bw > 255 ? 255 : bw;
    if (n > (unsigned char)(rx_head - rx_tail - 1))     /* free space */
        n = rx_head - rx_tail - 1;
    if (rx_tail && n > (unsigned char)-rx_tail)         /* room to the page end */
        n = -rx_tail;
    if (!n)
        return 0;                       /* ring full: the assembler drains it first */
    if (network_read(dev, rx_ring + rx_tail, n) != n)
        return NET_CLOSED;
    rx_tail += n;
    return 0;
}

unsigned char __fastcall__ net_send(unsigned char len)
{
    skip = 0;                           /* the answer is on its way: look at once */
    return network_write(dev, tx, len);
}

void net_close(void)
{
    network_close(dev);
}

/* The lobby's AppKey (creator 1, app 1) as a C string from rx_ring[2]; its length, 0 if none.
 * A missing key still reads as a success with a nonzero length (FujiNet 1.6 on SmartPort),
 * so the length is counted up to the first NUL. */
unsigned char __fastcall__ net_appkey(unsigned char key)
{
    unsigned len = 0;
    fuji_set_appkey_details(1, 1, DEFAULT);
    if (!fuji_read_appkey(key, &len, rx_ring + 2) || len > 64)
        len = 0;
    rx_ring[2 + len] = 0;
    for (n = 0; rx_ring[2 + n]; ++n)
        ;
    return n;
}
