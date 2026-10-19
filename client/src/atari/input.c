/* Keyboard and clock for the Atari. Reads the OS's last-key byte directly: no
 * K: handler, no key click. Shift and Control are ignored. The OPTION console
 * key is read from CONSOL once a frame (which rides out contact bounce), and
 * reported once per press. */
#include "../platform.h"

#define CH      (*(volatile unsigned char *)0x2FC)
#define ATRACT  (*(unsigned char *)0x4D)
#define RTCLOK  (*(volatile unsigned char *)0x14)
#define CONSOL  (*(volatile unsigned char *)0xD01F)  /* bit clear = key held */

/* Atari key code (low 6 bits) -> ASCII; 0 = a key the game doesn't use */
static const unsigned char keymap[64] = {
    'L', 'J', ';',  0,   0,  'K', '+', '*',  'O',  0,  'P', 'U', K_RETURN, 'I', '-', '=',
    'V',  0,  'C',  0,   0,  'B', 'X', 'Z',  '4',  0,  '3', '6', 0,          '5', '2', '1',
    ',', ' ', '.', 'N',  0,  'M', '/',  0,   'R',  0,  'E', 'Y', 0,          'T', 'W', 'Q',
    '9',  0,  '0', '7', K_DELETE, '8', '<', '>', 'F', 'H', 'D', 0,         0,   'G', 'S', 'A',
};

static unsigned char k, frame, held;

unsigned char in_key(void)
{
    if (frame != RTCLOK) {
        frame = RTCLOK;
        k = ~CONSOL & 4;                /* OPTION */
        if (k != held) {
            held = k;
            if (k) {
                ATRACT = 0;
                return K_OPTION;
            }
        }
    }
    k = CH;
    if (k == 0xFF)
        return 0;
    CH = 0xFF;
    ATRACT = 0;                         /* a key stops the colour-cycling screen saver */
    return keymap[k & 0x3F];
}

unsigned char ticks(void)
{
    return RTCLOK;
}
