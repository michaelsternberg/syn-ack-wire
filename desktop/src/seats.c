/* The seats taken at each region, for the Region menu: GET /SEATS from the
 * servers' web page (server/acquire/web.py), plain HTTP, on a thread of its own
 * so the menu never waits on the network. Each line of the answer is
 * "REGION TAKEN SEATS". */
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "desk.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define NO_SOCK INVALID_SOCKET
#define close_sock closesocket
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int sock_t;
#define NO_SOCK (-1)
#define close_sock close
#endif

#define MAX_REGIONS 16
#define WAIT_MS     4000                /* to connect, and for each read */
#define FRESH_MS    120000              /* older counts aren't shown */

struct count {
    char region[16];
    int taken, seats;
};

static SDL_Mutex *lock;
static SDL_AtomicInt busy;
static struct count counts[MAX_REGIONS];
static int ncounts;
static Uint64 got_at;                   /* when counts[] came, 0 = never */
static char host[256];
static char port[8];

/* The whole answer into buf, or -1. Blocking, with timeouts: this is the fetch thread. */
static int get(char *buf, int size)
{
    struct addrinfo hints, *list, *ai;
    sock_t s = NO_SOCK;
    int n = 0, r;
    char req[512];
#ifdef _WIN32
    DWORD ms = WAIT_MS;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#else
    struct timeval ms = { WAIT_MS / 1000, 0 };
#endif
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &list))
        return -1;
    for (ai = list; ai && s == NO_SOCK; ai = ai->ai_next) {
        s = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
        if (s == NO_SOCK)
            continue;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof ms);
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&ms, sizeof ms);
        if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) != 0) {
            close_sock(s);
            s = NO_SOCK;
        }
    }
    freeaddrinfo(list);
    if (s == NO_SOCK)
        return -1;
    SDL_snprintf(req, sizeof req, "GET /SEATS HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    if (send(s, req, (int)strlen(req), 0) != (int)strlen(req)) {
        close_sock(s);
        return -1;
    }
    while (n < size - 1 && (r = recv(s, buf + n, size - 1 - n, 0)) > 0)
        n += r;
    close_sock(s);
    buf[n] = 0;
    return n;
}

static int run(void *unused)
{
    static char buf[4096];
    struct count got[MAX_REGIONS];
    int n = 0;
    char *body;
    (void)unused;
    if (get(buf, sizeof buf) > 0 && strncmp(buf, "HTTP/1.", 7) == 0 && strncmp(buf + 9, "200", 3) == 0
        && (body = strstr(buf, "\r\n\r\n")) != 0) {
        char *line = body + 4, *next;
        for (; *line && n < MAX_REGIONS; line = next) {
            next = strchr(line, '\n');
            next = next ? next + 1 : line + strlen(line);
            if (SDL_sscanf(line, "%15s %d %d", got[n].region, &got[n].taken, &got[n].seats) == 3)
                ++n;
        }
        SDL_LockMutex(lock);
        memcpy(counts, got, sizeof got);
        ncounts = n;
        got_at = SDL_GetTicks();
        SDL_UnlockMutex(lock);
    }
    SDL_SetAtomicInt(&busy, 0);
    return 0;
}

void seats_fetch(const char *h, unsigned short p)
{
    if (!lock)
        lock = SDL_CreateMutex();
    if (!SDL_CompareAndSwapAtomicInt(&busy, 0, 1))
        return;                         /* one at a time */
    SDL_strlcpy(host, h, sizeof host);
    SDL_snprintf(port, sizeof port, "%u", p);
    SDL_DetachThread(SDL_CreateThread(run, "seats", 0));
}

unsigned long long seats_age(void)
{
    Uint64 at;
    if (!lock)
        return (Uint64)-1;
    SDL_LockMutex(lock);
    at = got_at;
    SDL_UnlockMutex(lock);
    return at ? SDL_GetTicks() - at : (Uint64)-1;
}

int seats_of(const char *region, int *taken, int *seats)
{
    int k, found = 0;
    if (!lock)
        return 0;
    SDL_LockMutex(lock);
    if (got_at && SDL_GetTicks() - got_at < FRESH_MS)
        for (k = 0; k < ncounts && !found; ++k)
            if (strcmp(counts[k].region, region) == 0) {
                *taken = counts[k].taken;
                *seats = counts[k].seats;
                found = 1;
            }
    SDL_UnlockMutex(lock);
    return found;
}
