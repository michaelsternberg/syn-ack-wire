/* Network calls for the desktop: one TCP connection, straight to the server,
 * where the 8-bit machines go through FujiNet. The server can't tell the
 * difference. The URL the common code passes is ignored: the server is the one
 * the Region menu picked (net_set_server). */
#include <string.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include "../../client/src/platform.h"
#include "desk.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define NO_SOCK INVALID_SOCKET
#define close_sock closesocket
#define WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#define IN_PROGRESS (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define NO_SOCK (-1)
#define close_sock close
#define WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#define IN_PROGRESS (errno == EINPROGRESS)
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0                  /* macOS: SO_NOSIGPIPE on the socket instead */
#endif

#define DIAL_MS 8000                    /* give up dialling after this: NO CARRIER */

unsigned char rx_ring[256], rx_head, rx_tail, tx[16];

static sock_t sock = NO_SOCK;
static char host[256] = "localhost";
static char port[8] = "6502";
static const char *handle = "";
static int (*aborted)(void);

void net_set_server(const char *h, unsigned short p)
{
    SDL_strlcpy(host, h, sizeof host);
    SDL_snprintf(port, sizeof port, "%u", p);
}

void net_set_abort(int (*abort)(void))
{
    aborted = abort;
}

void net_set_handle(const char *h)
{
    handle = h;
}

int net_is_open(void)
{
    return sock != NO_SOCK;
}

static void nonblocking(sock_t s)
{
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
#endif
}

/* Connects without blocking, so a Region change or Quit can interrupt a dial
 * that hangs (a server that doesn't answer). */
static sock_t dial(const struct addrinfo *ai)
{
    int one = 1, err = 0;
    Uint64 t0 = SDL_GetTicks();
    sock_t s = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (s == NO_SOCK)
        return NO_SOCK;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
#ifdef SO_NOSIGPIPE
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    nonblocking(s);
    if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) != 0 && !IN_PROGRESS) {
        close_sock(s);
        return NO_SOCK;
    }
    for (;;) {
        fd_set w, e;
        struct timeval tv = { 0, 100000 };
        socklen_t len = sizeof err;
        FD_ZERO(&w);
        FD_ZERO(&e);
        FD_SET(s, &w);
        FD_SET(s, &e);
        if (select((int)s + 1, 0, &w, &e, &tv) > 0) {
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
            if (err == 0 && FD_ISSET(s, &w))
                return s;
            break;
        }
        if (SDL_GetTicks() - t0 > DIAL_MS || (aborted && aborted()))
            break;
    }
    close_sock(s);
    return NO_SOCK;
}

unsigned char net_open(const char *url)
{
    struct addrinfo hints, *list, *ai;
    (void)url;
#ifdef _WIN32
    static int started;
    if (!started) {
        WSADATA wsa;
        started = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }
#endif
    net_close();
    rx_head = rx_tail = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &list))
        return 1;
    for (ai = list; ai && sock == NO_SOCK; ai = ai->ai_next)
        sock = dial(ai);
    freeaddrinfo(list);
    return sock == NO_SOCK ? 2 : 0;
}

/* As on the 8-bit machines: as much as the ring has room for, in one piece. */
unsigned char net_poll(void)
{
    unsigned char room = rx_head - rx_tail - 1, end = -rx_tail;
    int n;
    if (sock == NO_SOCK)
        return NET_CLOSED;
    if (rx_tail && end < room)
        room = end;
    if (!room)
        return 0;
    n = recv(sock, (char *)rx_ring + rx_tail, room, 0);
    if (n == 0)
        return NET_CLOSED;
    if (n < 0)
        return WOULD_BLOCK ? 0 : 1;
    rx_tail += n;
    return 0;
}

unsigned char net_send(unsigned char len)
{
    unsigned char done = 0;
    Uint64 t0 = SDL_GetTicks();
    if (sock == NO_SOCK)
        return NET_CLOSED;
    while (done < len) {
        int n = send(sock, (const char *)tx + done, len - done, MSG_NOSIGNAL);
        if (n > 0)
            done += n;
        else if (n < 0 && WOULD_BLOCK && SDL_GetTicks() - t0 < 2000)
            SDL_Delay(1);
        else
            return 1;
    }
    return 0;
}

void net_close(void)
{
    if (sock != NO_SOCK)
        close_sock(sock);
    sock = NO_SOCK;
}

/* No FujiNet game lobby here. Key 0, the player's lobby name, is the handle
 * typed last time, so the title screen starts with it as it does from the lobby. */
unsigned char net_appkey(unsigned char key)
{
    unsigned char n = 0;
    if (key == 0)
        while (handle[n] && n < 64) {
            rx_ring[2 + n] = handle[n];
            ++n;
        }
    rx_ring[2 + n] = 0;
    return n;
}
