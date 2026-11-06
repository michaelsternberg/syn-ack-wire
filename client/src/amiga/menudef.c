#include "../platform.h"
#include "keys.h"
#include "regions_tab.h"
#include "menudef.h"

static const struct menu_item project[] = {
    { "INSTRUCTIONS", 'I', K_HELP },
    { "QUIT", 'Q', K_ESC },             /* as Esc: the title and the game both ask first */
};
static const struct menu_item game[] = {
    { "MOVE HINTS", 'M', K_OPTION },
    { "SOUND", 'S', AK_SOUND },
    { "SEATS AS TABLE", 'T', AK_TABLE },
};
static struct menu_item region_items[NREGIONS];
static const struct menu_def menus[3] = {
    { "PROJECT", sizeof project / sizeof project[0], project },
    { "GAME", sizeof game / sizeof game[0], game },
    { "REGION", NREGIONS, region_items },
};
static unsigned char filled;

static void fill(void)
{
    unsigned char n;
    for (n = 0; n < NREGIONS; ++n) {
        region_items[n].label = regions[n].name;
        region_items[n].commkey = '1' + n;
        region_items[n].key = AK_REGION + n;
    }
    filled = 1;
}

unsigned char menu_count(void)
{
    return 3;
}

const struct menu_def *menu_def(unsigned char menu)
{
    if (!filled)
        fill();
    return menu < 3 ? &menus[menu] : 0;
}

unsigned char menu_item_key(unsigned char menu, unsigned char item)
{
    const struct menu_def *m = menu_def(menu);
    return m && item < m->n ? m->items[item].key : 0;
}
