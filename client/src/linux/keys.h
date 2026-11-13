/* Curses keys as the game's keys (platform.h), as src/msdos/input.c has them. Tested on the host:
 * make test-linux-keys. */
#ifndef NL_KEYS_H
#define NL_KEYS_H

/* the keys input.c acts on itself, never handed to the game (above ASCII) */
#define NL_KEY_SOUND  0x81              /* F3, Ctrl-S */

unsigned char nl_keymap(int ch);        /* the game's key, an NL_KEY_*, or 0 for none */

#endif
