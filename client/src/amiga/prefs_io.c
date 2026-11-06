/* The prefs file, S:SynAckWire.prefs: the handle, read at the start for the title and written when the player dials, and
 * an optional SERVER= address, dialled instead of the built-in server (and kept when the handle is written). */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../common/game.h"
#include "amiga.h"
#include "prefs.h"

static char known[HANDLE_MAX + 1];       /* the handle in the file, as last read or written */
static char known_server[SERVER_MAX + 1];   /* its server address, "" for none */
static char picked[SERVER_MAX + 1];         /* the Region menu's choice this run, "" for none: it beats the file */
static char saved_server[SERVER_MAX + 1];   /* the address the file has */
static struct prefs_set saved_set = { 0, 0, 1 };   /* the settings the file has: the defaults until it says otherwise */

/* No "insert volume" requester on the Workbench screen behind the game: a failed open just fails. */
static APTR quiet_dos(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR was = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    return was;
}

static void current(struct prefs_set *now)
{
    now->view = amiga_table_view();
    now->hints = hints;
    now->sound = amiga_snd_on();
}

/* The lobby's AppKey 0 (the player's name) is here the saved handle, from rx_ring[2]; the lobby's own key (LOBBY_KEY, the
 * server it picked) is here the SERVER= address, which main.c dials after "N:"; any other key is none. */
unsigned char net_appkey(unsigned char key)
{
    char file[PREFS_FILE_MAX + 13];
    BPTR f;
    LONG got = 0;
    unsigned char n;
    APTR was;

    rx_ring[2] = 0;
    if (key != 0 && key != LOBBY_KEY)
        return 0;
    if (key != 0 && picked[0]) {
        n = (unsigned char)strlen(picked);
        memcpy(rx_ring + 2, picked, n + 1);
        return n;
    }
    was = quiet_dos();
    f = Open((UBYTE *)PREFS_FILE, MODE_OLDFILE);
    if (f) {
        got = Read(f, file, sizeof file);
        Close(f);
    }
    ((struct Process *)FindTask(0))->pr_WindowPtr = was;
    if (!f || got <= 0)
        return 0;
    prefs_server(file, (unsigned char)got, known_server);
    strcpy(saved_server, known_server);
    if (key == 0) {                     /* the first look, at the start: the kept settings take effect */
        current(&saved_set);            /* a setting the file lacks stays as the build has it */
        prefs_settings(file, (unsigned char)got, &saved_set);
        hints = saved_set.hints;
        amiga_snd_set(saved_set.sound);
        amiga_table_set(saved_set.view);
    }
    if (key != 0) {
        n = (unsigned char)strlen(known_server);
        memcpy(rx_ring + 2, known_server, n + 1);
        return n;
    }
    n = prefs_parse(file, (unsigned char)got, (char *)rx_ring + 2);
    memcpy(known, rx_ring + 2, n + 1);
    return n;
}

/* Remember the handle just played under. Best effort: a locked or full disk loses nothing but the memory. */
const char *amiga_server(void)
{
    return known_server;
}

/* The Region menu's pick: dialled next, and written with the handle at the next jack-in. */
void amiga_set_server(const char *url)
{
    strncpy(picked, url, SERVER_MAX);
    picked[SERVER_MAX] = 0;
    strcpy(known_server, picked);
}

static unsigned char same_set(const struct prefs_set *a, const struct prefs_set *b)
{
    return a->view == b->view && a->hints == b->hints && a->sound == b->sound;
}

/* Write the file for `name`, the server and the settings as they are now, unless it already says that. Best effort: a locked
 * or full disk loses nothing but the memory. */
static void write_file(const char *handle)
{
    char file[PREFS_FILE_MAX + 1], name[9];
    struct prefs_set now;
    unsigned char i;
    BPTR f;
    APTR was;

    current(&now);
    i = prefs_format(handle, known_server, &now, file);
    file[i] = 0;
    prefs_parse(file, i, name);         /* the handle as the file would give it back */
    if (strcmp(name, known) == 0 && strcmp(known_server, saved_server) == 0 && same_set(&now, &saved_set))
        return;                         /* nothing new: spare the floppy */
    was = quiet_dos();
    if ((f = Open((UBYTE *)PREFS_FILE, MODE_NEWFILE)) != 0) {
        Write(f, file, i);
        Close(f);
        strcpy(known, name);
        strcpy(saved_server, known_server);
        saved_set = now;
    }
    ((struct Process *)FindTask(0))->pr_WindowPtr = was;
}

/* Remember the handle just played under (and the settings). */
void amiga_prefs_save(void)
{
    char name[9];
    unsigned char i;

    if (!edit_len)
        return;
    for (i = 0; i < edit_len && i < HANDLE_MAX; ++i)
        name[i] = edit[i];
    name[i] = 0;
    write_file(name);
}

/* On the way out: a setting changed in play (hints, sound, the seats' view) is kept too. */
void amiga_prefs_exit(void)
{
    struct prefs_set now;
    current(&now);
    if (!same_set(&now, &saved_set))
        write_file(known);
}
