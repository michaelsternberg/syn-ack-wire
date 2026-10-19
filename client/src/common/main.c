/* The client's main loop: title, connect, then frames in, keys out, forever.
 * A dropped link reconnects with the seat token; the server resends everything. */
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "proto.h"
#include "proto_tab.h"
#include "game.h"

unsigned char token[4];
static unsigned char handle[8];

static unsigned char c, e, t0, secs;

#ifdef AUTOPLAY
unsigned char auto_key(void);           /* test/autoplay.c */
#define in_key auto_key
#endif

static void wait_ticks(unsigned char n)
{
    t0 = ticks();
    while ((unsigned char)(ticks() - t0) < n)
        ;
}

/* Title: type a handle, Return to jack in. */
static void title(void)
{
    edit_len = 0;
    display_title_screen();
    for (;;) {
        c = in_key();
        if (c == K_RETURN && edit_len)
            return;
        if (c == '*')
            hints ^= 1;
        else if (c == K_DELETE && edit_len)
            --edit_len;
        else if (c > ' ' && c <= 'Z' && edit_len < 8)
            edit[edit_len++] = c;
        else
            continue;
        display_title_screen();
    }
}

static void hello(void)
{
    tx[0] = 'H';
    tx[1] = PROTO_VERSION;
    tx[2] = TABLE;
    for (c = 0; c < 4; ++c)
        tx[3 + c] = token[c];
    for (c = 0; c < 8; ++c)
        tx[7 + c] = handle[c];
    net_send(1 + CLEN_H);
}

static void connect(void)
{
    display_message("DIALING " URL);
    while ((e = net_open(URL)) != 0) {
        display_message("NO CARRIER. REDIALING...");
        net_close();
        wait_ticks(2 * TICKS_PER_SEC);
    }
    hello();
}

void main(void)
{
    title();
    for (c = 0; c < 8; ++c)
        handle[c] = c < edit_len ? edit[c] : ' ';
    connect();
    for (;;) {
        if ((e = net_poll()) != 0) {
            display_message("CARRIER LOST. REDIALING...");
            net_close();
            wait_ticks(2 * TICKS_PER_SEC);
            connect();
            continue;
        }
        while ((c = proto_next()) != 0) {
            if (c == PROTO_BAD) {       /* lost our place in the stream */
                tx[0] = 'R';
                net_send(1);
                break;
            }
            if (c == 'f') {
                if (msg[1] == F_TOKEN) {        /* the server forgot us: join as new */
                    for (e = 0; e < 4; ++e)
                        token[e] = 0;
                    net_close();
                    connect();
                    break;
                }
                if (msg[1] == F_FULL)
                    display_message("TABLE FULL: GAME IN PROGRESS");
                else if (msg[1] == F_VERSION)
                    display_message("SERVER SPEAKS ANOTHER VERSION");
                if (msg[1] != F_ILLEGAL)
                    for (;;)
                        ;
                continue;
            }
            game_frame();
            if (c == 'p') {
                secs = pr[0];
                t0 = ticks();
            }
        }
        c = in_key();                   /* always read, so no stale key answers the next prompt */
        if (c == '*') {
            game_toggle_hints();
            c = 0;
        }
        if (!pr_kind)
            continue;
        if (c)
            game_key(c);
        if (secs && (unsigned char)(ticks() - t0) >= TICKS_PER_SEC) {
            t0 += TICKS_PER_SEC;
            display_clock(--secs);
            if (!secs)
                game_clear_prompt();    /* time's up: the seat's bot answers */
        }
    }
}
