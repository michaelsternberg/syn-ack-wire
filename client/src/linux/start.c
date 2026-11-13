/* The program's own main for linux: options, then the checks that give a clear message before curses takes
 * the screen, then the game (src/common/main.c, built with main renamed game_main). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "nl.h"
#include "screen.h"

char nl_server_url[80];

void game_main(void);

static void usage(FILE *f)
{
    fprintf(f, "usage: syn-ack-wire-linux [--port PTY] [--server HOST:PORT]\n"
               "  --port PTY         the PTY fujinet-nio made (else $FN_PORT, else " NL_DEFAULT_PTY ")\n"
               "  --server HOST:PORT a game server to dial (else $SYNACK_URL, else the public one)\n"
               "Start fujinet-nio first: see client/README.md.\n");
}

/* "host:port" as "N:TCP://host:port/" in nl_server_url; 0 if it doesn't look like one */
static int set_server(const char *hp)
{
    const char *colon = strrchr(hp, ':');
    const char *p;
    if (!colon || colon == hp || !colon[1] || strlen(hp) + sizeof "N:TCP:///" > sizeof nl_server_url)
        return 0;
    for (p = colon + 1; *p; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    snprintf(nl_server_url, sizeof nl_server_url, "N:TCP://%s/", hp);
    return 1;
}

int main(int argc, char **argv)
{
    const char *port, *server = getenv("SYNACK_URL");
    int i, fd;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(argv[i], "--port") && i + 1 < argc)
            setenv("FN_PORT", argv[++i], 1);
        else if (!strcmp(argv[i], "--server") && i + 1 < argc)
            server = argv[++i];
        else {
            usage(stderr);
            return 2;
        }
    }
    if (server && *server && !set_server(server)) {
        fprintf(stderr, "syn-ack-wire: --server wants HOST:PORT, not \"%s\"\n", server);
        return 2;
    }
    if (!isatty(0) || !isatty(1)) {
        fprintf(stderr, "syn-ack-wire: needs a terminal (standard input and output must be one)\n");
        return 1;
    }
    port = getenv("FN_PORT");
    if (!port || !*port) {
        port = NL_DEFAULT_PTY;
        setenv("FN_PORT", port, 1);
    }
    fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "syn-ack-wire: cannot open PTY %s: start fujinet-nio first, see client/README.md\n", port);
        return 1;
    }
    if (!isatty(fd)) {
        fprintf(stderr, "syn-ack-wire: %s is not a terminal (a PTY): start fujinet-nio first, see client/README.md\n", port);
        return 1;
    }
    close(fd);
    nl_open();                          /* curses on; from here on stderr goes to /dev/null */
    game_main();
    return 0;
}
