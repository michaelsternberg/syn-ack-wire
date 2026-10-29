/* The game thread: the client's common main() (built as game_main), keys from
 * the window, the clock, and the window's requests to dial another server.
 *
 * main() never returns and waits in busy loops, as on the 8-bit machines, so it
 * runs on a thread of its own. It sees the window's requests where it reads a
 * key or the clock: a Region change hangs up, logs off first if asked, and
 * starts main() again from the title screen (a longjmp back to the top). */
#include <setjmp.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "../../client/src/config.h"
#include "../../client/src/platform.h"
#include "../../client/src/common/proto_tab.h"
#include "../../client/src/common/game.h"
#include "desk.h"

void game_main(void);                   /* client/src/common/main.c */
extern unsigned char token[4];          /* main.c: the seat's token, zero until seated */

static SDL_Mutex *lock;
static unsigned char keys[64], key_head, key_tail;

static SDL_AtomicInt switching;         /* a Region change waits */
static char next_host[256];
static unsigned short next_port;
static int next_log_off;

static char handle[9];                  /* for net_appkey(0) after a restart */
static jmp_buf top;
static unsigned spins;                  /* ticks() calls since the last key look */

/* ------------------------------------------------------------------ keys */

void key_push(unsigned char k)
{
    SDL_LockMutex(lock);
    if ((unsigned char)(key_tail + 1) % sizeof keys != key_head) {
        keys[key_tail] = k;
        key_tail = (key_tail + 1) % sizeof keys;
    }
    SDL_UnlockMutex(lock);
}

static void check_switch(void);

/* F1-F3 and the menus arrive as platform.h's codes, or K_SOUND, done here as
 * the MS-DOS client does F3. With no key waiting, a millisecond's rest, so
 * the loops that call this don't spin a core. */
unsigned char in_key(void)
{
    unsigned char k = 0;
    check_switch();
    spins = 0;
    SDL_LockMutex(lock);
    if (key_head != key_tail) {
        k = keys[key_head];
        key_head = (key_head + 1) % sizeof keys;
    }
    SDL_UnlockMutex(lock);
    if (!k)
        SDL_Delay(1);
    if (k == K_SOUND) {
        snd_toggle();
        return 0;
    }
    return k;
}

/* 60 a second. A loop that only watches the clock (wait_ticks) rests too. */
unsigned char ticks(void)
{
    check_switch();
    if (++spins > 8)
        SDL_Delay(1);
    return (unsigned char)(SDL_GetTicks() * 60 / 1000);
}

unsigned char ticks_per_sec(void)
{
    return 60;
}

/* Esc, then Y on the title screen: the window closes, as File > Quit does. */
void platform_quit(void)
{
    desk_request_quit();
    for (;;)
        SDL_Delay(100);
}

/* ------------------------------------------------------------------ Region changes */

static int switch_waiting(void)
{
    return SDL_GetAtomicInt(&switching);
}

void game_switch(const char *host, unsigned short port, int log_off)
{
    SDL_LockMutex(lock);
    SDL_strlcpy(next_host, host, sizeof next_host);
    next_port = port;
    next_log_off = log_off;
    SDL_UnlockMutex(lock);
    SDL_SetAtomicInt(&switching, 1);
}

int game_seated(void)
{
    return (token[0] | token[1] | token[2] | token[3]) != 0;
}

/* The handle: typed on the title screen (edit[]), or once seated, the seat's
 * name, as edit[] then holds the answers to prompts. */
static void current_handle(char *h)
{
    unsigned char n = 0;
    if (game_seated() && me < SEATS)
        for (; n < 8 && seat_name[me][n] != ' '; ++n)
            h[n] = seat_name[me][n];
    else
        for (; n < edit_len; ++n)
            h[n] = edit[n];
    h[n] = 0;
}

int game_hints(void)
{
    return hints;
}

/* As main.c's log_off() does after Esc, Y: Q, so the bot takes the seat for
 * good; then hang up, and back to the title of the other server as a newcomer.
 * The handle typed so far comes back there. */
static void check_switch(void)
{
    if (!switch_waiting())
        return;
    SDL_LockMutex(lock);
    net_set_server(next_host, next_port);
    if (next_log_off && game_seated() && net_is_open()) {
        tx[0] = 'Q';
        net_send(1 + CLEN_Q);
    }
    SDL_SetAtomicInt(&switching, 0);
    SDL_UnlockMutex(lock);
    net_close();
    current_handle(handle);
    memset(token, 0, sizeof token);
    link_up = leaving = refused = 0;
    pr_kind = 0;
    region_len = 0;
    status_len = 0;
    edit_len = 0;
    display_reset();
    longjmp(top, 1);
}

/* ------------------------------------------------------------------ the thread */

static int run(void *unused)
{
    (void)unused;
    setjmp(top);
    game_main();
    return 0;
}

void game_start(const struct game_setup *s)
{
    lock = SDL_CreateMutex();
    net_set_server(s->host, s->port);
    net_set_abort(switch_waiting);
    SDL_strlcpy(handle, s->handle, sizeof handle);
    net_set_handle(handle);
    memcpy(token, s->token, sizeof token);
    hints = s->hints != 0;
    snd_set(s->sound);
    display_set_data_dir(s->data_dir);
    SDL_DetachThread(SDL_CreateThread(run, "game", 0));
}

void game_state(char *h, unsigned char *t, int *hn, int *sound)
{
    current_handle(h);
    memcpy(t, token, sizeof token);
    *hn = hints;
    *sound = snd_is_on();
}
