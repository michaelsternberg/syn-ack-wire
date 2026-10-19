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

#ifndef URL
#define URL "N:TCP://localhost:6502/"
#endif
#ifndef TABLE
#define TABLE 0
#endif

#endif
