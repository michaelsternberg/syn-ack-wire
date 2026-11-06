/* The pull-down menus (right mouse button), Kickstart 1.3 Intuition structures built from menudef.c's table. A pick is the
 * key the keyboard would type, except the Region ones, which choose the server the title dials next. The strip is only
 * on the window while no window of the game's own is up: then the right button is the game's (it cancels), as before. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <string.h>
#include "../platform.h"
#include "../common/game.h"
#include "amiga.h"
#include "keys.h"
#include "menudef.h"
#include "regions_tab.h"
#include "prefs.h"
#include "../display.h"

#define MAXITEMS 8
#define ITEM_H 10
#define CHECK_W 19                      /* room the check mark takes at the left of an item */
#define COMM_W 40                       /* and the Amiga-key letter at the right */

static struct Menu strip[3];
static struct MenuItem items[3][MAXITEMS];
static struct IntuiText texts[3][MAXITEMS];
static unsigned char built, attached, last_state;

static unsigned char is_toggle(unsigned char m, unsigned char i)
{
    return m == 1 && i < 3;             /* the Game menu's three */
}

static void build(void)
{
    unsigned char m, i, x = 0;
    for (m = 0; m < 3; ++m) {
        const struct menu_def *d = menu_def(m);
        unsigned char w = 0;
        for (i = 0; i < d->n; ++i) {
            const struct menu_item *it = &d->items[i];
            unsigned char len = (unsigned char)strlen(it->label), t = is_toggle(m, i);
            texts[m][i].FrontPen = 0;
            texts[m][i].BackPen = 1;
            texts[m][i].DrawMode = JAM1;
            texts[m][i].LeftEdge = t ? CHECK_W : 0;
            texts[m][i].TopEdge = 1;
            texts[m][i].IText = (UBYTE *)it->label;
            items[m][i].LeftEdge = 0;
            items[m][i].TopEdge = i * ITEM_H;
            items[m][i].Width = len * 8 + (t ? CHECK_W : 0) + (it->commkey ? COMM_W : 8);
            items[m][i].Height = ITEM_H;
            items[m][i].Flags = ITEMTEXT | ITEMENABLED | HIGHCOMP | (it->commkey ? COMMSEQ : 0) | (t ? CHECKIT | MENUTOGGLE : 0);
            items[m][i].ItemFill = &texts[m][i];
            items[m][i].Command = it->commkey;
            items[m][i].NextItem = i + 1 < d->n ? &items[m][i + 1] : 0;
            if (items[m][i].Width > w)
                w = items[m][i].Width;
        }
        for (i = 0; i < d->n; ++i)
            items[m][i].Width = w;
        strip[m].NextMenu = m < 2 ? &strip[m + 1] : 0;
        strip[m].LeftEdge = x;
        strip[m].Width = (unsigned char)strlen(d->name) * 8 + 16;
        strip[m].Flags = MENUENABLED;
        strip[m].MenuName = (CONST_STRPTR)d->name;
        strip[m].FirstItem = &items[m][0];
        x += strip[m].Width;
    }
    built = 1;
}

/* what the strip shows: bit 0 hints, 1 sound, 2 table, 3 Region usable (the title is up), 4 the strip is on the window */
static unsigned char state_now(unsigned char allowed)
{
    return (hints ? 1 : 0) | (amiga_snd_on() ? 2 : 0) | (amiga_table_view() ? 4 : 0) | (amiga_title_up() ? 8 : 0) | (allowed ? 16 : 0);
}

static void apply_flags(unsigned char st)
{
    unsigned char i;
    for (i = 0; i < 3; ++i) {
        if (st >> i & 1)
            items[1][i].Flags |= CHECKED;
        else
            items[1][i].Flags &= ~CHECKED;
    }
    for (i = 0; i < NREGIONS && i < MAXITEMS; ++i) {
        if (st & 8)
            items[2][i].Flags |= ITEMENABLED;
        else
            items[2][i].Flags &= ~ITEMENABLED;
    }
}

static unsigned char allowed(void)
{
    /* off, too, while the link is down and the title is not asking: a dial or a redial waits on the network, and a menu
     * opened then would hold the whole machine's input until it ended */
    return !dialog_up() && !amiga_helping() && !dialog_quit_open() && (link_up || amiga_title_up());
}

/* Bring the strip to the state of the game. The strip is changed only while off the window, and the window's RMBTRAP says
 * who owns the right button: the menus, or (under a window of ours) the game, whose right click cancels. */
static void sync(unsigned char want)
{
    unsigned char st = state_now(want);
    if (!amiga_win)
        return;
    if (!built)
        build();
    if (attached && st == last_state)
        return;
    if (attached) {
        ClearMenuStrip(amiga_win);
        attached = 0;
    }
    apply_flags(st);
    if (want) {
        amiga_win->Flags &= ~RMBTRAP;
        SetMenuStrip(amiga_win, strip);
        attached = 1;
    } else
        amiga_win->Flags |= RMBTRAP;
    last_state = st;
}

void menus_update(void)
{
    sync(allowed());
}

void menus_refresh(void)
{
    menus_update();                     /* just before a menu opens: the ticks follow the keys that changed them */
}

unsigned short menus_next(unsigned short code)
{
    struct MenuItem *it;
    if (!amiga_win || !attached || code == MENUNULL)
        return MENUNULL;
    it = ItemAddress(strip, code);
    return it ? it->NextSelect : MENUNULL;
}

void menus_close(void)
{
    if (amiga_win && attached)
        ClearMenuStrip(amiga_win);
    attached = 0;
}

/* the title's Region menu: the next dial goes to that server, and the screen says so */
void menus_region(unsigned char n)
{
    char url[SERVER_MAX + 1], was_url[SERVER_MAX + 1], t[6], url_n[SERVER_MAX + 3];
    unsigned char was_region[12], was_len;
    unsigned char i, j = 5;
    unsigned short p;
    if (n >= NREGIONS || !amiga_title_up())
        return;
    p = regions[n].port;
    t[5] = 0;
    do {
        t[--j] = '0' + p % 10;
        p /= 10;
    } while (p);
    strcpy(url, "TCP://" REGION_HOST ":");
    strcat(url, t + j);
    strcat(url, "/");
    strcpy(url_n, "N:");
    strcat(url_n, url);
    strcpy(was_url, amiga_server());
    memcpy(was_region, region, sizeof region);
    was_len = region_len;
    amiga_set_server(url);
    for (i = 0; regions[n].name[i] && i < sizeof region; ++i)
        region[i] = regions[n].name[i];
    region_len = i;
    status_len = status_code = 0;
    if (link_up)
        net_close();
    link_up = 0;
    if (net_open(url_n)) {              /* unreachable: keep the server that was, or Return would redial this one for ever */
        amiga_set_server(was_url);
        memcpy(region, was_region, sizeof region);
        region_len = was_len;
        status_code = NO_CARRIER;
        status_len = 0;
    } else {
        net_close();                    /* it answers; the next Return dials it */
    }
    display_title_update();
}
