/* Keyboard and clock for the Atari. Reads the OS's last-key byte directly: no
 * K: handler, no key click. Shift and Control are ignored, so the arrow keys
 * arrive as + * - =, except Shift-/ (?), which is HELP for the 400/800; the
 * HELP key (XL/XE) comes from the OS's HELPFG. The OPTION and
 * SELECT console keys are read from CONSOL once a frame (which rides out contact
 * bounce), and act once per press: OPTION is reported, SELECT turns sound on
 * or off right here, wherever the game is reading keys. */
#include "../platform.h"

#define CH      (*(volatile unsigned char *)0x2FC)
#define ATRACT  (*(unsigned char *)0x4D)
#define RTCLOK  (*(volatile unsigned char *)0x14)
#define CONSOL  (*(volatile unsigned char *)0xD01F)  /* bit clear = key held */
#define HELPFG  (*(volatile unsigned char *)0x2DC)
#define PAL     (*(volatile unsigned char *)0xD014)  /* GTIA: low bits %0001 on PAL, %1111 on NTSC */

/* Atari key code (low 6 bits) -> ASCII; 0 = a key the game doesn't use */
static const unsigned char keymap[64] = {
    'L', 'J', ';',  0,   0,  'K', '+', '*',  'O',  0,  'P', 'U', K_RETURN, 'I', '-', '=',
    'V',  0,  'C',  0,   0,  'B', 'X', 'Z',  '4',  0,  '3', '6', K_ESC,      '5', '2', '1',
    ',', ' ', '.', 'N',  0,  'M', '/',  0,   'R',  0,  'E', 'Y', 0,          'T', 'W', 'Q',
    '9',  0,  '0', '7', K_DELETE, '8', '<', '>', 'F', 'H', 'D', 0,         0,   'G', 'S', 'A',
};

static unsigned char k, frame, held, pressed;

unsigned char in_key(void)
{
    if (frame != RTCLOK) {
        frame = RTCLOK;
        k = ~CONSOL & 6;                /* OPTION, SELECT */
        pressed = k & ~held;
        held = k;
        if (pressed & 2)
            snd_toggle();
        if (pressed & 4) {
            ATRACT = 0;
            return K_OPTION;
        }
    }
    if (HELPFG) {
        HELPFG = 0;
        ATRACT = 0;
        return K_HELP;
    }
    k = CH;
    if (k == 0xFF)
        return 0;
    CH = 0xFF;
    ATRACT = 0;                         /* a key stops the colour-cycling screen saver */
    if (k == 0x66)                      /* Shift-/ */
        return K_HELP;
    return keymap[k & 0x3F];
}

unsigned char ticks(void)
{
    return RTCLOK;
}

unsigned char ticks_per_sec(void)
{
    return PAL & 0x0E ? 60 : 50;
}
