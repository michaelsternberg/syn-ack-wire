/* Keys from the game window's IDCMP_RAWKEY messages, and the frame counter from the vertical blank. */
#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <devices/console.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <hardware/intbits.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "../platform.h"
#include "amiga.h"
#include "keys.h"
#include "gfx.h"
#include "ui.h"
#include "menudef.h"

#define RING 32

static unsigned char ring[RING];
static unsigned char ring_head, ring_tail;

static volatile unsigned char tick;
static struct Interrupt vbl;
static struct MsgPort *con_port;
static struct IOStdReq *con_io;
struct Device *ConsoleDevice;
static UBYTE vbl_added, con_open;

static unsigned long vbl_server(void)   /* VERTB: Z set (d0 = 0) lets the chain go on */
{
    ++tick;
    return 0;
}

unsigned char amiga_input_open(void)
{
    vbl.is_Node.ln_Type = NT_INTERRUPT;
    vbl.is_Node.ln_Name = (char *)"SYN-ACK WIRE";
    vbl.is_Code = (void (*)())vbl_server;
    AddIntServer(INTB_VERTB, &vbl);
    vbl_added = 1;

    con_port = CreatePort(0, 0);
    if (con_port) {
        con_io = (struct IOStdReq *)CreateExtIO(con_port, sizeof *con_io);
        if (con_io && OpenDevice((UBYTE *)"console.device", (ULONG)-1, (struct IORequest *)con_io, 0) == 0) {
            ConsoleDevice = con_io->io_Device;
            con_open = 1;
        }
    }
    return con_open;
}

void amiga_input_close(void)
{
    if (con_open)
        CloseDevice((struct IORequest *)con_io);
    if (con_io)
        DeleteExtIO((struct IORequest *)con_io);
    if (con_port)
        DeletePort(con_port);
    if (vbl_added)
        RemIntServer(INTB_VERTB, &vbl);
    con_open = vbl_added = 0;
    con_io = 0;
    con_port = 0;
    ConsoleDevice = 0;
}

unsigned char ticks(void)
{
    return tick;
}

unsigned char ticks_per_sec(void)
{
    return SysBase->VBlankFrequency == 50 ? 50 : 60;
}

static void push(unsigned char k)
{
    unsigned char next = (ring_head + 1) & (RING - 1);

    if (next != ring_tail) {            /* a full ring drops the key rather than block the game */
        ring[ring_head] = k;
        ring_head = next;
    }
}

/* A key from the keyboard, a click or a menu: the ones that are not the game's (sound, the seats' view, a Region) are done
 * here and never reach it; the rest are queued for it. */
static void act(unsigned char k)
{
    if (dialog_holding() && k != AK_SOUND && k != AK_TABLE && ak_region(k) < 0) {
        amiga_over_release();           /* the scores were looked at: this key only ends the wait */
        return;
    }
    if (k == AK_SOUND)                  /* F3, or the SOUND button, or its menu item */
        snd_toggle();
    else if (k == AK_TABLE)
        amiga_toggle_table();
    else if (ak_region(k) >= 0)
        menus_region(ak_region(k));
    else {
        k = ui_key(k);
        if (k == K_RETURN && amiga_title_up())
            amiga_title_leave();        /* the join starts: the Region menu is off from here on */
        push(k);
    }
}

/* A mouse press is the keys the keyboard would type for the widget under it (ui.c). One press is taken at a time: the
 * second of a double click within a few ticks is dropped (ui_press_ok), and so is any press while the key ring is half
 * full, so a held or hammered button never queues a flood of keys that the game then replays. */
void amiga_click(unsigned char button, short x, short y)
{
    unsigned char keys[UI_KEYS_MAX], n, i;

    if (!ui_press_ok(tick, (ring_head - ring_tail) & (RING - 1), RING))
        return;
    if (dialog_holding()) {
        amiga_over_release();
        ui_press_taken(tick);
        return;
    }
    if (button == UI_LEFT && amiga_layout.ntsc) {       /* the seat list's arrows */
        if (layout_inside(&amiga_layout.scroll_up, x, y) && amiga_scroll_others(-1))
            return;
        if (layout_inside(&amiga_layout.scroll_down, x, y) && amiga_scroll_others(1))
            return;
    }
    n = ui_click(&amiga_layout, x, y, button, keys);
    for (i = 0; i < n; ++i)
        act(keys[i]);
    if (ui_dirty())
        dialog_sync();                  /* a stepper moved */
    if (n)
        ui_press_taken(tick);
}

static void take_rawkey(UWORD code, UWORD qualifier, APTR address)
{
    struct InputEvent ie;
    char buf[4];
    unsigned char k;
    LONG n = 0;

    if (code & 0x80)
        return;
    if (con_open) {
        memset(&ie, 0, sizeof ie);
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_Code = code;
        ie.ie_Qualifier = qualifier;
        ie.ie_EventAddress = address;
        n = RawKeyConvert(&ie, (UBYTE *)buf, sizeof buf, 0);
    }
    k = amiga_key_translate(code, qualifier, n == 1 ? buf[0] : 0);
    if (k)
        act(k);
}

unsigned char in_key(void)
{
    struct IntuiMessage *m;
    unsigned char k;

    ui_press_age(tick);
    if (amiga_win) {
        while ((m = (struct IntuiMessage *)GetMsg(amiga_win->UserPort)) != 0) {
            ULONG class = m->Class;
            UWORD code = m->Code, qualifier = m->Qualifier;
            APTR address = m->IAddress;
            WORD mx = m->MouseX, my = m->MouseY;

            if (class == IDCMP_MENUVERIFY)
                menus_refresh();        /* before the reply, which lets the menu open */
            ReplyMsg((struct Message *)m);
            if (class == IDCMP_MENUPICK) {
                while (code != MENUNULL) {      /* a chain: each item picked, in order */
                    act(menu_item_key(MENUNUM(code), ITEMNUM(code)));
                    code = menus_next(code);
                }
            } else if (class == IDCMP_RAWKEY)
                take_rawkey(code, qualifier, address);
            else if (class == IDCMP_MOUSEBUTTONS && code == SELECTDOWN)
                amiga_click(UI_LEFT, mx, my);
            else if (class == IDCMP_MOUSEBUTTONS && code == MENUDOWN)
                amiga_click(UI_RIGHT, mx, my);
        }
    }
    if (ring_head == ring_tail)
        return 0;
    k = ring[ring_tail];
    ring_tail = (ring_tail + 1) & (RING - 1);
    return k;
}
