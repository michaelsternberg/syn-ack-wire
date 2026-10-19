/* The platform.h network calls on top of fujinet-lib. Kept for the size bake-off (see README);
 * the Atari build uses src/atari/net_sio.s. Likely the route on Apple II / CoCo. */
#include <stdint.h>
#include <stdbool.h>
#include <fujinet-network.h>
#include "../src/platform.h"

unsigned char rx_head, rx_tail;
unsigned char tx[16];

static const char *dev;
static uint16_t bw;
static uint8_t conn, err;
static unsigned char n, room;

unsigned char __fastcall__ net_open(const char *url)
{
    dev = url;
    rx_head = rx_tail = 0;
    return network_open(url, OPEN_MODE_RW, OPEN_TRANS_NONE);
}

unsigned char net_poll(void)
{
    if (network_status(dev, &bw, &conn, &err))
        return NET_CLOSED;
    if (!bw)
        return conn ? 0 : NET_CLOSED;
    n = bw > 255 ? 255 : bw;
    room = rx_head - rx_tail - 1;
    if (n > room)
        n = room;
    if (rx_tail) {
        room = -rx_tail;
        if (n > room)
            n = room;
    }
    if (!n)
        return 0;
    if (network_read_nb(dev, rx_ring + rx_tail, n) != n)
        return NET_CLOSED;
    rx_tail += n;
    return 0;
}

unsigned char __fastcall__ net_send(unsigned char len)
{
    return network_write(dev, tx, len);
}

void net_close(void)
{
    network_close(dev);
}
