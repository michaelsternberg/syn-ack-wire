/* A mouse click becomes the keys the keyboard would type for the open prompt; game.c decides whether they are legal.
 * Pure: it reads the game state (game.h) and the layout, and never writes either. Tested on the host. */
#ifndef AMIGA_UI_H
#define AMIGA_UI_H

#include "layout.h"

#define UI_LEFT  1
#define UI_RIGHT 2
#define UI_KEYS_MAX 6                   /* a hivejack's two digits and Return, after up to two Deletes */

/* The keys for a press of `button` at (x, y), written to keys[]; returns how many (0: the click means nothing). */
unsigned char ui_click(const struct layout *l, short x, short y, unsigned char button, unsigned char *keys);

void ui_title_mode(unsigned char on);   /* the title screen is up: its buttons answer, not the playfield */
unsigned char ui_key(unsigned char k);   /* a typed key as the game gets it: Return is Yes on a Y/N question (the lit button) */
void ui_ask_yes_no(unsigned char on);   /* a Y/N question is on screen (the title's quit): YES is Y, any other click N */
void ui_help_mode(unsigned char on);    /* the instructions are up: a click turns the page or leaves, nothing else answers */
/* The debounce: a press is taken only if the key ring (of `ring` slots, `queued` used) is under half full, and not
 * within UI_PRESS_GAP ticks of the last one that made keys. ticks are the 8-bit frame counter and may wrap. */
#define UI_PRESS_GAP 3
unsigned char ui_press_ok(unsigned char now, unsigned char queued, unsigned char ring);
void ui_press_taken(unsigned char now);
void ui_press_forget(void);
void ui_press_age(unsigned char now);      /* called every pass: the gap is over, so a later wrap cannot look like a recent press */

void ui_reset(void);                    /* forget the hivejack stepper */
unsigned char ui_dirty(void);           /* the dialog changed (a stepper moved): draw it again; clears the flag */
unsigned char ui_count(void);           /* the hivejack stepper's count, for drawing it */

#endif
