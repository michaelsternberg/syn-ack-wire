/* The platform.h network calls for linux: POSIX FujiNet-NIO over its PTY, through fujinet-nio-lib's linux
 * target (the PTY comes from FN_PORT). One TCP session to the server. Every call returns 0 when fine, else the
 * library's error code or NET_CLOSED. As src/amiga/net_fn.c, which this follows: there is no "data waiting"
 * signal, so while nothing comes net_poll asks about 10 times a second, and while bytes arrive, and after
 * sending, at every call.
 *
 * A TCP session's offsets must run in sequence: roff and woff count the bytes read and written so far.
 * nl_server_url (start.c: --server or SYNACK_URL), when set, is dialled instead of the URL main.c passes. */
#include <stdlib.h>
#include <string.h>
#include <fujinet-nio.h>
#include "../platform.h"

unsigned char rx_ring[256];
unsigned char rx_head, rx_tail, tx[16];

#define READ_CAP   128                  /* one read: small, as the RS-232 link's replies are the fragile part */
#define GRACE_SECS 3                    /* failed exchanges this long in a row: the line is dead */

static fn_handle_t sess;
static uint32_t roff, woff;
static unsigned char is_open, inited, quiet, asked, failing, fail_t, eof;
static char host[64];
static unsigned char exit_hook;

extern char nl_server_url[80];

/* An exchange that failed (a reply garbled or lost) is let go and asked again; only failing for GRACE_SECS counts
 * as a dropped line, as on MS-DOS: a blip of a second once ended the connection and the bot took the seat's move. */
static unsigned char failed(unsigned char code)
{
    if (!failing) {
        failing = 1;
        fail_t = ticks();
    }
    quiet = 1;
    return (unsigned char)(ticks() - fail_t) >= (unsigned char)(GRACE_SECS * ticks_per_sec()) ? code : 0;
}

/* "N:TCP://host:port/": the host, and the port (1-65535); 0 when it doesn't parse */
static unsigned parse(const char *url)
{
    const char *p = url, *colon;
    unsigned long port = 0;
    unsigned len;
    if (p[0] == 'N' && p[1] == ':')
        p += 2;
    if (strncmp(p, "TCP://", 6))
        return 0;
    p += 6;
    colon = strchr(p, ':');
    if (!colon || colon == p || (unsigned)(colon - p) >= sizeof host)
        return 0;
    len = (unsigned)(colon - p);
    memcpy(host, p, len);
    host[len] = 0;
    for (p = colon + 1; *p >= '0' && *p <= '9'; ++p) {
        port = port * 10 + (unsigned)(*p - '0');
        if (port > 65535UL)
            return 0;
    }
    return (unsigned)port;
}

static unsigned char open_session(unsigned port)
{
    unsigned char rc;
    if (!inited) {
        if ((rc = fn_init()) != FN_OK)
            return rc;
        inited = 1;
    }
    if ((rc = fn_tcp_open(&sess, host, port)) != FN_OK)
        return rc;
    is_open = 1;
    if (!exit_hook) {                   /* a quit by signal leaves the session in fujinet-nio, which holds four */
        exit_hook = 1;
        atexit(net_close);
    }
    return 0;
}

unsigned char net_open(const char *url)
{
    unsigned port = parse(nl_server_url[0] ? nl_server_url : url);
    if (is_open) {                      /* a redial: release the old session first */
        fn_close(sess);
        is_open = 0;
    }
    rx_head = rx_tail = 0;              /* anything left from an old connection is junk */
    roff = woff = 0;
    quiet = failing = eof = 0;
    if (!port)
        return 1;
    if (inited) {
        fn_shutdown();
        inited = 0;
    }
    return open_session(port);
}

unsigned char net_poll(void)
{
    uint16_t got = 0;
    uint8_t flags = 0, rc;
    unsigned char room = rx_head - rx_tail - 1, n;
    if (!is_open)
        return NET_CLOSED;
    if (eof)                            /* the stream has ended: closed once the assembler has what came */
        return rx_head == rx_tail ? NET_CLOSED : 0;
    if (quiet && (unsigned char)(ticks() - asked) < (unsigned char)(ticks_per_sec() / 10 ? ticks_per_sec() / 10 : 1))
        return 0;
    asked = ticks();
    n = room;
    if (rx_tail && n > (unsigned char)-rx_tail)         /* room to the ring's end */
        n = -rx_tail;
    if (n > READ_CAP)
        n = READ_CAP;
    if (!n)
        return 0;                       /* ring full: the assembler drains it first */
    rc = fn_read(sess, roff, rx_ring + rx_tail, n, &got, &flags);
    if (rc == FN_ERR_NOT_READY) {
        failing = 0;                    /* the exchange itself worked: an earlier blip is over */
        quiet = 1;
        return 0;
    }
    if (rc != FN_OK)
        return failed(rc);
    failing = 0;
    quiet = !got;
    rx_tail += (unsigned char)got;
    roff += got;
    if (flags & FN_READ_EOF) {
        eof = 1;
        return got || rx_head != rx_tail ? 0 : NET_CLOSED;
    }
    return 0;
}

unsigned char net_send(unsigned char len)
{
    uint16_t w = 0;
    uint8_t rc;
    quiet = 0;                          /* the answer is on its way: look at once */
    rc = fn_write(sess, woff, tx, len, &w);
    woff += w;
    return rc ? rc : (w == len ? 0 : 1);
}

void net_close(void)
{
    if (is_open)
        fn_close(sess);
    is_open = 0;
}


/* A PC has no FujiNet lobby: no AppKey, so the client dials the built-in URL (or nl_server_url). */
unsigned char __fastcall__ net_appkey(unsigned char key)
{
    (void)key;
    rx_ring[2] = 0;
    return 0;
}
