/* Build-time choices. The Makefile can override any of these (make FANCY=1 URL=...). */
#ifndef CONFIG_H
#define CONFIG_H

/* 0 = Claude's text-only screen, 1 = the hand-made one. Each screen switches
 * on its own, so a fancy title can arrive before a fancy playfield. */
#ifndef FANCY_TITLE
#define FANCY_TITLE     0
#endif
#ifndef FANCY_PLAYFIELD
#define FANCY_PLAYFIELD 0
#endif

/* Show, on each empty node an exploit in your kit can run on, that exploit's
 * key (1-6). The player can flip it with OPTION on the title screen and in the game. */
#ifndef HINTS
#define HINTS 1
#endif

/* The server to dial, unless the FujiNet game lobby picked one: it stores the
 * server's address ("TCP://host:port/") in AppKey creator 1, app 1, key LOBBY_KEY,
 * the number the server registers with the lobby as its `appkey` (1-255).
 * LOBBY_KEY 0: always dial URL and leave the handle empty. */
#ifndef URL
#define URL "N:TCP://syn-ack-wire.fly.dev:6502/"
#endif
#ifndef LOBBY_KEY
#define LOBBY_KEY 11
#endif
#ifndef TABLE
#define TABLE 0
#endif

/* 1 = the game loop calls display_idle() on every pass, for a display that
 * changes with time (apple2_text: the last move flashes for a few seconds). */
#ifndef DISPLAY_IDLE
#define DISPLAY_IDLE 0
#endif

#endif
