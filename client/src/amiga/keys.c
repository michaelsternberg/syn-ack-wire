#include "../platform.h"
#include "keys.h"

#define Q_CTRL   0x0008
#define Q_AMIGA  0x00C0                 /* either Amiga key */

unsigned char amiga_key_translate(unsigned short code, unsigned short qualifier, char ascii)
{
    unsigned char c;

    if (code & 0x80)
        return 0;
    switch (code) {
    case 0x50: return K_HELP;           /* F1 */
    case 0x51: return K_OPTION;         /* F2 */
    case 0x52: return AK_SOUND;         /* F3 */
    case 0x53: return AK_TABLE;         /* F4 */
    case 0x5F: return K_HELP;           /* Help */
    case 0x41: case 0x46: return K_DELETE;
    case 0x43: case 0x44: return K_RETURN;
    case 0x45: return K_ESC;
    case 0x4F: return '<';
    case 0x4E: return '>';
    }
    if (qualifier & (Q_CTRL | Q_AMIGA))
        return 0;
    c = (unsigned char)ascii;
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    if (c == '?')
        return K_HELP;
    return c >= 0x20 && c <= 0x5F ? c : 0;
}
