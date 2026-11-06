/* The unattended client's answers as mouse clicks (test/autoplay.c with -DAUTOCLICK): the key the autoplayer would press
 * for the open prompt becomes a press in the middle of the widget that gives it, through the same amiga_click() path the
 * IDCMP mouse message takes. A key with no widget (a wrong one, the typed handle) is left to go as a key. Each click is
 * logged, so the acceptance script can count the prompts that were answered by the mouse. */
#include <stdio.h>
#include "../src/platform.h"
#include "../src/common/game.h"
#include "../src/amiga/amiga.h"
#include "../src/amiga/gfx.h"
#include "../src/amiga/ui.h"

static void press(const struct rect *r)
{
    amiga_click(UI_LEFT, r->x + r->w / 2, r->y + r->h / 2);
}

static signed char syn_index(unsigned char letter)
{
    unsigned char i;
    for (i = 0; i < NSYN; ++i)
        if (syn_letter[i] == letter)
            return i;
    return -1;
}

unsigned char amiga_autoclick(unsigned char key)
{
    const struct layout *l = &amiga_layout;
    const struct rect *r = 0;
    signed char i;
    unsigned char n;

    if (!pr_kind) {                                 /* the title */
        r = key == '\r' ? &l->btn[BTN_JACK] : key == K_HELP ? &l->btn[BTN_HELP] : 0;
    } else {
        switch (pr_kind) {
        case 'S':
            r = key >= '2' && key <= '6' ? &l->btn[BTN_SEAT2 + key - '2'] : 0;
            break;
        case 'P':
            r = key >= '1' && key <= '6' ? &l->kit_chip[key - '1'] : 0;
            break;
        case 'F':
        case 'K':
            r = (i = syn_index(key)) >= 0 ? &l->syn_row[i] : 0;
            break;
        case 'B':
            if (step)
                r = key == K_RETURN || key == 'N' ? &l->btn[BTN_NO] : 0;
            else if (key == K_RETURN)
                r = &l->btn[BTN_BUY];
            else if (key == K_DELETE)
                r = &l->btn[BTN_UNDO];
            else if ((i = syn_index(key)) >= 0)
                r = &l->dlg_row[i];
            break;
        case 'D':
            if (key == K_RETURN || (key >= '0' && key <= '9')) {
                for (n = key == K_RETURN ? 0 : key - '0'; n; --n)
                    press(&l->btn[BTN_PLUS]);
                r = &l->btn[BTN_OK];
            }
            break;
        }
    }
    if (!r)
        return 0;
    printf("click %c %02x\n", pr_kind ? pr_kind : 'T', key);
    fflush(stdout);
    press(r);
    return 1;
}
