/* An Amiga raw key to a platform.h key; pure, so it is tested on the host. */
#ifndef AMIGA_KEYS_H
#define AMIGA_KEYS_H

#define AK_SOUND 0x03                   /* F3: handled by in_key, never reaches the game */
#define AK_TABLE 0x04                   /* F4, or a click on the seats: cards or table, done by in_key */
#define AK_REGION 0x80                  /* + n: the Region menu's n-th server, chosen on the title */
#define AK_REGION_MAX 16
/* which Region pick k is, or -1 if it is a key for the game */
#define ak_region(k) ((k) >= AK_REGION && (k) < AK_REGION + AK_REGION_MAX ? (int)(k) - AK_REGION : -1)

/* code: the IDCMP raw key code (bit 7 set is a key up), qualifier: IEQUALIFIER_*,
 * ascii: what the console device made of the key (0 if nothing). Returns 0 for a key the game ignores. */
unsigned char amiga_key_translate(unsigned short code, unsigned short qualifier, char ascii);

#endif
