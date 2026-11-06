/* The menu strip as a table (pure, tested on the host): the names, the Amiga-key letters and the key each pick stands for.
 * menus.c builds the Intuition strip from it. */
#ifndef AMIGA_MENUDEF_H
#define AMIGA_MENUDEF_H

struct menu_item {
    const char *label;
    char commkey;                       /* the letter after the Amiga key, 0 for none */
    unsigned char key;                  /* what a pick types: a platform.h K_*, AK_SOUND, AK_TABLE or AK_REGION + n */
};
struct menu_def {
    const char *name;
    unsigned char n;
    const struct menu_item *items;
};

unsigned char menu_count(void);
const struct menu_def *menu_def(unsigned char menu);                    /* 0 past the last */
unsigned char menu_item_key(unsigned char menu, unsigned char item);    /* 0 for no such item (or MENUNULL's 31 and 63) */

#endif
