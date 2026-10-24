/* Run the client's common code (main.c, game.c, proto.c) on a PC with gcc:
 * sockets instead of FujiNet, a display that prints what it would draw, and
 * test/autoplay.c's keys. Finds logic bugs without an emulator.
 *
 *   make pc && build/pc/pc-client [host] [port]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "../src/platform.h"
#include "../src/display.h"
#include "../src/common/proto.h"
#include "../src/common/game.h"

unsigned char rx_ring[256], rx_head, rx_tail, tx[16];
static int sock = -1;
static const char *host = "127.0.0.1", *port = "6502";

unsigned char __fastcall__ net_open(const char *url)
{
    struct addrinfo *ai;
    (void)url;
    rx_head = rx_tail = 0;
    if (getaddrinfo(host, port, 0, &ai))
        return 1;
    sock = socket(ai->ai_family, SOCK_STREAM, 0);
    if (connect(sock, ai->ai_addr, ai->ai_addrlen))
        return 2;
    fcntl(sock, F_SETFL, O_NONBLOCK);
    return 0;
}

unsigned char net_poll(void)
{
    unsigned char room = rx_head - rx_tail - 1, end = -rx_tail;
    int n;
    if (rx_tail && end < room)
        room = end;
    if (!room)
        return 0;
    n = recv(sock, rx_ring + rx_tail, room, 0);
    if (n == 0)
        return NET_CLOSED;
    if (n > 0)
        rx_tail += n;
    return 0;
}

unsigned char __fastcall__ net_send(unsigned char len)
{
    fprintf(stderr, "  >> %c", tx[0]);
    for (int i = 1; i < len; ++i)
        fprintf(stderr, " %02x", tx[i]);
    fprintf(stderr, "\n");
    return send(sock, tx, len, 0) == len ? 0 : 1;
}

void net_close(void)
{
    close(sock);
}

unsigned char __fastcall__ net_appkey(unsigned char key)
{
    (void)key;
    rx_ring[2] = 0;                     /* no game lobby here */
    return 0;
}

unsigned char in_key(void)
{
    return 0;
}

unsigned char ticks(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (unsigned char)((tv.tv_sec * 1000000LL + tv.tv_usec) / 16667);
}

unsigned char ticks_per_sec(void)
{
    return 60;
}

void snd_init(void) {}
void play_chord(unsigned char n) { (void)n; }
void snd_toggle(void) {}

/* ---- display: print what changed */
void display_title_screen(void) {}
void display_title_update(void) {}
void display_playfield(void) { fprintf(stderr, "[playfield] turn %d me %d\n", turn, me); }
void display_node(unsigned char node) { (void)node; }
void display_hives(void) {}
void display_seat(unsigned char seat) { (void)seat; }
void display_log(void) { fprintf(stderr, "[log] %.*s\n", msg[1], msg + 2); }
void display_prompt(void)
{
    if (pr_kind)
        fprintf(stderr, "[prompt %c] step %d edit '%.*s' %s\n", pr_kind, step, edit_len, edit,
                complaint ? complaint : "");
}
void display_clock(unsigned char secs) { (void)secs; }
void display_message(const char *s) { fprintf(stderr, "[msg] %s\n", s); }
void display_game_over(void) { fprintf(stderr, "[game over] winners %02x\n", winners); if (turn == NONE && pr_kind == 0) exit(0); }
void help_open(void) { }
unsigned char help_key(unsigned char k) { return k != K_ESC; }
void display_link(void) { fprintf(stderr, "[link] %s\n", link_up ? "CONNECTED" : "NO SIGNAL"); }

int harness_args(int argc, char **argv)
{
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = argv[2];
    return 0;
}
