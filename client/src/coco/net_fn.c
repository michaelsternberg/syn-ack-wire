/* The platform.h network calls for the CoCo 1/2: FujiNet over DriveWire (the
 * serial port, or a Becker port in an emulator), through fujinet-lib
 * (lib/fujinet-lib, a pinned copy). Every call returns 0 when fine, else
 * fujinet-lib's error code or NET_CLOSED.
 *
 * Asking FujiNet for its status costs a DriveWire exchange, with interrupts
 * off, so net_poll asks only about 10 times a second while nothing comes, and
 * at once while bytes arrive or an answer is due. On the serial port FujiNet
 * also signals waiting bytes on the carrier detect line (the Atari's PROCEED);
 * a change on it sets PIA1's CA1 flag, which says to ask now. It is only a
 * hint: a Becker port has no such line, so the paced asking stays. */
#include <fujinet-network.h>
#include <fujinet-fuji.h>
#include "../platform.h"

#define PIA1_PA  (*(unsigned char *)0xFF20)  /* reading it clears CA1's flag */
#define PIA1_CRA (*(unsigned char *)0xFF21)  /* bit 7: CA1 (carrier detect) changed */
#define POLL_TICKS 6                    /* 60ths of a second between asks while nothing comes */

unsigned char rx_ring[256], rx_head, rx_tail, tx[16];

static const char dev[] = "N:";         /* the unit; the URL is only needed to open */
static unsigned bw;
static unsigned char conn, err, n;      /* err: FujiNet's own code, 1 = fine */
static unsigned char flowing, asked;    /* bytes came last time / when it last asked */

/* A failed open may still answer fine: the failure shows in the status after
 * it (not connected, FujiNet's error code). */
unsigned char __fastcall__ net_open(const char *url)
{
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
    if (!flowing) {
        if (!(PIA1_CRA & 0x80) && (unsigned char)(ticks() - asked) < POLL_TICKS)
            return 0;
        n = PIA1_PA;                    /* clears the flag */
    }
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
    if (network_read(dev, rx_ring + rx_tail, n) != n)
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
 * as a success. */
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
