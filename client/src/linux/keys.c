/* Curses keys to the game's keys: see keys.h. */
#include <curses.h>
#include "../platform.h"
#include "keys.h"

unsigned char nl_keymap(int ch)
{
    switch (ch) {
    case KEY_F(1): case '?':        return K_HELP;
    case KEY_F(2): case 9:          return K_OPTION;    /* 9 is Tab */
    case KEY_F(3): case 19:         return NL_KEY_SOUND;    /* 19 is Ctrl-S (flow control is off) */
    case KEY_BACKSPACE: case 127: case 8: case KEY_DC:
                                    return K_DELETE;
    case KEY_LEFT:                  return '<';     /* page turns in the instructions */
    case KEY_RIGHT:                 return '>';
    case KEY_UP:                    return K_UP;
    case KEY_DOWN:                  return K_DOWN;
    case '\n': case '\r': case KEY_ENTER:
                                    return K_RETURN;
    case 27:                        return K_ESC;
    }
    if (ch >= 'a' && ch <= 'z')
        return (unsigned char)(ch - 'a' + 'A');
    if (ch >= ' ' && ch < 0x7F)
        return (unsigned char)ch;
    return 0;
}
