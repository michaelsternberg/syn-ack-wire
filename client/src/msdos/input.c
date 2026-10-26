/* Keyboard and clock for MS-DOS. Keys come from the BIOS (INT 16h). The
 * function keys stand in for the Atari's console keys: F1 the instructions (as
 * HELP, and ? as on the others), F2 the move hints (OPTION), F3 sound on or off
 * (SELECT), switched right here, wherever the game is reading keys. F4 swaps 40
 * and 80 columns, which the display allows only on the title screen; F5 colour
 * on or off.
 *
 * The clock is the BIOS tick at 0040:006C, 18.2 a second, which the timer
 * interrupt keeps counting while a serial call to FujiNet waits: so
 * ticks_per_sec() is 18, and a countdown's second is a few percent short. */
#include <bios.h>
#include <stdlib.h>
#include "../platform.h"

#define BIOS_TICK (*(volatile unsigned char __far *)0x0040006CL)

/* BIOS scan codes of the keys that have no ASCII */
#define SCAN_F1  0x3B
#define SCAN_F2  0x3C
#define SCAN_F3  0x3D
#define SCAN_F4  0x3E
#define SCAN_F5  0x3F
#define SCAN_DEL 0x53
#define SCAN_LEFT  0x4B
#define SCAN_RIGHT 0x4D

void snd_poll(void);                    /* sound.c: an AdLib chord's time, checked with every key */
void display_width(void);               /* display_text.c: 40 <-> 80 columns, on the title screen */
void display_colour(void);              /* display_text.c: green phosphor on or off */
void display_restore(void);             /* display_text.c: the video mode DOS had */
void snd_off(void);                     /* sound.c: silence */

unsigned char in_key(void)
{
    unsigned short k;
    unsigned char c;
    snd_poll();
    if (!_bios_keybrd(_KEYBRD_READY))
        return 0;
    k = _bios_keybrd(_KEYBRD_READ);
    c = (unsigned char)k;
    if (!c || c == 0xE0) {              /* an extended key: look at the scan code */
        switch (k >> 8) {
        case SCAN_F1:  return K_HELP;
        case SCAN_F2:  return K_OPTION;
        case SCAN_F3:  snd_toggle(); return 0;
        case SCAN_F4:  display_width(); return 0;
        case SCAN_F5:  display_colour(); return 0;
        case SCAN_DEL: return K_DELETE;
        case SCAN_LEFT:  return '<';     /* page turns in the instructions */
        case SCAN_RIGHT: return '>';
        }
        return 0;
    }
    if (c == '?')
        return K_HELP;
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    return c;                           /* Enter is '\r', Backspace '\b', Esc 0x1B: the platform.h codes */
}

/* Esc and Y on the title screen: silence, the screen as DOS had it, and back to DOS. */
void platform_quit(void)
{
    snd_off();
    display_restore();
    exit(0);
}

unsigned char ticks(void)
{
    return BIOS_TICK;
}

unsigned char ticks_per_sec(void)
{
    return 18;
}
