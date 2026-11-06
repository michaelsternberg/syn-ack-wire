/* Before main() and after it: the libraries, the screen, the keys. The destructor runs on every way out
 * (main returning, exit()), closes only what opened, and puts things back in reverse order. */
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdlib.h>
#include "../platform.h"
#include "amiga.h"

#define LIB_V33 33                      /* Kickstart 1.2; 1.3 is 34: nothing newer is used */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static unsigned char input_open, display_open;

__attribute__((constructor)) static void amiga_init(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((UBYTE *)"intuition.library", LIB_V33);
    GfxBase = (struct GfxBase *)OpenLibrary((UBYTE *)"graphics.library", LIB_V33);
    if (IntuitionBase && GfxBase && amiga_display_open()) {
        display_open = 1;
        input_open = 1;
        amiga_input_open();             /* without console.device the keys are only the unshifted raw ones */
        return;
    }
    exit(20);                           /* the destructor undoes what did open */
}

__attribute__((destructor)) static void amiga_exit(void)
{
    amiga_prefs_exit();                 /* a setting changed in play is kept (before the windows go) */
    if (input_open)
        amiga_input_close();
    if (display_open || IntuitionBase)
        amiga_display_close();
    display_open = input_open = 0;
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    GfxBase = 0;
    IntuitionBase = 0;
}

/* Esc, Y on the title: the destructor above closes everything on the way out. */
void platform_quit(void)
{
    exit(0);
}
