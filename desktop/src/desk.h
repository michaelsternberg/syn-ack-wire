/* Between the window (C++, the main thread) and the client's common code with
 * this platform's half of it (C, the game thread). The game thread runs the
 * common main() as the 8-bit machines do; it draws into scr[] and reads keys
 * from a queue. The window shows scr[] and fills the queue. */
#ifndef DESK_H
#define DESK_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ the screen
 * 80 x 25 cells, as on MS-DOS in 80 columns: the circuit-board art in columns
 * 0-18 and 61-79, the game's 40 columns at 20-59. A cell is a code page 437
 * character and a role; the colour theme gives each role its colours. */
#define SCR_ROWS 25
#define SCR_COLS 80
#define GAME_LEFT 20

enum {
    R_TEXT,                             /* normal text */
    R_PANEL,                            /* under the Grid's nodes and the seats' rows */
    R_INV,                              /* reverse */
    R_BAR,                              /* the title bar */
    R_ASK,                              /* rows 22-23 while this seat decides */
    R_LAST,                             /* the last move, lit */
    R_ART,                              /* the circuit-board art */
    R_BRIGHT,                           /* the instructions' QR code */
    R_HIVE,                             /* R_HIVE + syndicate id: a hive's letter */
    NROLES = R_HIVE + 7
};

struct cell {
    unsigned char ch, role;
};

extern struct cell scr[SCR_ROWS][SCR_COLS];
void scr_lock(void);
void scr_unlock(void);
void display_reset(void);               /* the instructions closed, the title next: before a restart */
void display_set_data_dir(const char *dir);  /* where SYNACK.HLP is, ending in a separator */

/* ------------------------------------------------------------------ keys */
#define K_SOUND 0x03                    /* F3, Options > Sound: sound on or off */
#define K_NONE  0x04                    /* no key: closes the instructions that a restart left open */
void key_push(unsigned char k);

/* ------------------------------------------------------------------ the game thread */
struct game_setup {
    const char *host;                   /* the server */
    unsigned short port;
    const char *handle;                 /* the handle last typed, or "" */
    const unsigned char *token;         /* 4 bytes: a seat to take back, or zeros */
    int hints, sound;
    const char *data_dir;               /* where SYNACK.HLP is, ending in a separator */
};
void game_start(const struct game_setup *s);
/* Hang up and go to the title screen of another server, logging off first if
 * asked and there is a seat; done by the game thread at its next look. */
void game_switch(const char *host, unsigned short port, int log_off);
int game_seated(void);                  /* holds a seat at the table (a token) */
int game_hints(void);                   /* move hints are on */
void game_state(char *handle, unsigned char *token, int *hints, int *sound);  /* to save on the way out */

/* in the window (app.cpp): Esc, then Y on the title screen */
void desk_request_quit(void);

/* ------------------------------------------------------------------ network (net.c) */
void net_set_server(const char *host, unsigned short port);
int net_is_open(void);
void net_set_abort(int (*abort)(void));  /* polled while dialling: nonzero gives up */
void net_set_handle(const char *h);     /* net_appkey(0): the handle typed last time */

/* ------------------------------------------------------------------ seats at each region (seats.c) */
void seats_fetch(const char *host, unsigned short port);  /* GET /SEATS there, in the background */
unsigned long long seats_age(void);     /* milliseconds since the last answer; huge if none */
int seats_of(const char *region, int *taken, int *seats);  /* 1 if a recent answer has the region */

/* ------------------------------------------------------------------ sound (sound.c) */
int snd_is_on(void);
void snd_set(int on);

#ifdef __cplusplus
}
#endif

#endif
