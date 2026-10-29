/* The pad and the clock for the NES. The game asks for keys (platform.h's
 * in_key); the NES has a pad, so this turns the pad's presses (pad.s, latched
 * by the NMI) into the keys a keyboard would type, with a cursor on the
 * answers the open prompt can take:
 *
 *  A        the answer the cursor is on: the exploit's 1-6, the syndicate's
 *           letter, the seat count; YES at a yes/no question
 *  B        Delete (a bought letter, the handle's letter); NO at a yes/no
 *  START    Return: join, buy, a count confirmed
 *  d-pad    moves the cursor: an exploit in the kit, an offered syndicate, a
 *           syndicate in the hives panel while buying; up and down change a
 *           count (a hivejack's cash-out and swap, the host's seats)
 *  SELECT   the instructions (on its own, as it is let go); in them the d-pad
 *           turns the pages, and A, B or START go back
 *  SELECT+A move hints, SELECT+B sound, SELECT+START Esc (log off, Y/N)
 *
 * The cursor is `pick` (the display draws it, display_pick); it goes to the
 * first answer that can be taken when a prompt opens. A count is typed for the
 * game as edit[] holds it: it is written there, as typing would. On the title
 * the d-pad is a dial: up and down step the letter it is on (A-Z, 0-9, then the
 * signs a handle can have), left and right move along the handle, B deletes
 * the letter. All of it is in this file; the common code sees only keys.
 *
 * ticks() is the NMI's frame count; ticks_per_sec() what start-up measured
 * (crt0.s): 60, or 50 on a PAL machine. */
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"
#include "text.h"

#define P_A      0x80
#define P_B      0x40
#define P_SEL    0x20
#define P_START  0x10
#define P_UP     0x08
#define P_DOWN   0x04
#define P_LEFT   0x02
#define P_RIGHT  0x01
#define P_DIRS   0x0F

#define REPEAT_FIRST 20                 /* frames a direction is held before it repeats */
#define REPEAT_NEXT  6

extern unsigned char pad_ring[8], pad_head, pad_tail, pad_now;   /* pad.s */
extern unsigned char field;             /* display_text.c: the playfield is up */
extern unsigned char help_up;           /* help.c */
void display_pick(void);                /* display_text.c */

unsigned char pick;                     /* the answer the cursor is on */
unsigned char dial;                     /* the title: the handle's letter the dial is on */

static unsigned char ev, rep, rep_t, kind, kstep, count, sold, k, lim;

unsigned char ticks(void)
{
    return frames;
}

unsigned char ticks_per_sec(void)
{
    return nes_tps;
}

/* The next press, or a held direction's repeat; 0 if none. */
static unsigned char next(void)
{
    if (pad_tail != pad_head) {
        ev = pad_ring[pad_tail];
        pad_tail = (pad_tail + 1) & 7;
        if (ev & P_DIRS && !(ev & P_SEL)) {
            rep = ev & P_DIRS;
            rep_t = frames + REPEAT_FIRST;
        }
        return ev;
    }
    if (!(pad_now & rep))
        rep = 0;
    else if ((signed char)(frames - rep_t) >= 0) {
        rep_t = frames + REPEAT_NEXT;
        return ev = rep;
    }
    return 0;
}

/* ---------------------------------------------------------------- the title's dial */

static unsigned char step_letter(unsigned char c, unsigned char up)
{
    if (up) {
        switch (c) {
        case 'Z': return '0';
        case '9': return '!';
        case '/': return ':';
        case '@': return 'A';
        }
        return c + 1;
    }
    switch (c) {
    case 'A': return '@';
    case ':': return '/';
    case '!': return '9';
    case '0': return 'Z';
    }
    return c - 1;
}

static unsigned char title_key(void)
{
    if (dial > edit_len)
        dial = edit_len;
    if (dial > 7)
        dial = 7;
    switch (ev) {
    case P_UP:
    case P_DOWN:
        if (dial == edit_len)           /* past the end: a new letter */
            return ev == P_UP ? 'A' : 'Z';
        edit[dial] = step_letter(edit[dial], ev == P_UP);
        break;
    case P_LEFT:
        if (dial)
            --dial;
        break;
    case P_A:
    case P_RIGHT:
        if (dial < edit_len)
            ++dial;
        break;
    case P_B:
        if (!edit_len)
            return 0;
        if (dial < edit_len)            /* the letters after it close up; main.c drops the last */
            for (k = dial; k + 1 < edit_len; ++k)
                edit[k] = edit[k + 1];
        else
            --dial;
        return K_DELETE;
    case P_START:
        return K_RETURN;
    default:
        return 0;
    }
    display_title_update();
    return 0;
}

/* ---------------------------------------------------------------- the game's prompts */

/* Answer i of the open prompt can be taken. */
static unsigned char ok(unsigned char i)
{
    switch (kind) {
    case 'P':
        return kit[i] != NONE;
    case 'F':
        return pr[1] >> i & 1;
    case 'K':
        return pr[2] >> i & 1;
    case 'B':
        return pr[4 + i] != 0;          /* the syndicate has a hive: a price */
    }
    return 0;
}

/* The cursor to the next answer that can be taken, round the end. */
static void move(unsigned char up)
{
    lim = kind == 'P' ? RULE_KITSIZE : NSYN;
    for (k = 0; k < lim; ++k) {
        if (up)
            pick = pick + 1 == lim ? 0 : pick + 1;
        else
            pick = pick ? pick - 1 : lim - 1;
        if (ok(pick))
            break;
    }
    display_pick();
}

/* The count in edit[], as if typed, and the prompt drawn again. */
static void set_count(void)
{
    edit_len = 0;
    for (k = '0', lim = count; lim >= 10; lim -= 10)
        ++k;
    if (k != '0')
        edit[edit_len++] = k;
    edit[edit_len++] = '0' + lim;
    complaint = 0;
    display_prompt();
}

/* The most a hivejack's count can be: all held to cash out; then an even
 * number of what is left, for no more than twice the Darkpool's supply. */
static unsigned char count_max(void)
{
    /* pr: timeout, absorbed, absorber, held, supply */
    if (!step)
        return pr[3];
    lim = pr[3] - sold;
    if (lim > 2 * pr[4])
        lim = 2 * pr[4];
    return lim & ~1;
}

/* A prompt opened (or moved on a step): the cursor to its first answer. */
static void fresh(void)
{
    kind = pr_kind;
    kstep = step;
    switch (kind) {
    case 'S':
        for (pick = k = 0; k < SEATS; ++k)
            pick += seat_kind[k] != SEAT_EMPTY;
        if (pick < RULE_MINPLAYERS)
            pick = RULE_MINPLAYERS;
        display_prompt();
        return;
    case 'D':
        count = 0;
        set_count();
        return;
    case 'P':                           /* the first exploit that can run, else the first there */
        for (pick = 0; pick < RULE_KITSIZE; ++pick)
            if (kit[pick] != NONE && !(kit[pick] & 0x80))
                break;
        if (pick < RULE_KITSIZE) {
            display_pick();
            return;
        }
        /* fall through */
    default:
        pick = (kind == 'P' ? RULE_KITSIZE : NSYN) - 1;
        move(1);
    }
}

static unsigned char game_pad(void)
{
    if (leaving)                        /* log off? A: yes; B or START: stay */
        return ev == P_A ? 'Y' : ev & (P_B | P_START) ? 'N' : 0;
    if (!pr_kind)
        return 0;
    if (!ok(pick) && kind != 'S' && kind != 'D')
        move(1);                        /* the kit or the offer changed under it */
    switch (kind) {
    case 'S':
    case 'D':
        if (ev & (P_UP | P_RIGHT)) {
            if (kind == 'S') {
                if (pick < RULE_MAXPLAYERS)
                    ++pick;
                display_prompt();
            } else {
                count += step ? 2 : 1;
                if (count > count_max())
                    count = count_max();
                set_count();
            }
            return 0;
        }
        if (ev & (P_DOWN | P_LEFT)) {
            if (kind == 'S') {
                if (pick > RULE_MINPLAYERS)
                    --pick;
                display_prompt();
            } else {
                k = step ? 2 : 1;
                count = count > k ? count - k : 0;
                set_count();
            }
            return 0;
        }
        if (ev & (P_A | P_START)) {
            if (kind == 'S')
                return '0' + pick;
            if (!step)
                sold = count;
            return K_RETURN;
        }
        return 0;
    case 'B':
        if (step)                       /* the null-route: A yes, B no */
            return ev == P_A ? 'Y' : ev == P_B ? 'N' : ev == P_START ? K_RETURN : 0;
        break;
    }
    if (ev & (P_UP | P_LEFT))
        move(0);
    else if (ev & (P_DOWN | P_RIGHT))
        move(1);
    else if (ev == P_A)
        return kind == 'P' ? '1' + pick : syn_letter[pick];
    else if (ev == P_B)
        return K_DELETE;
    else if (ev == P_START)
        return K_RETURN;
    return 0;
}

unsigned char in_key(void)
{
    if (field && (pr_kind != kind || step != kstep))
        fresh();
    if (!next())
        return 0;
    if (ev == P_SEL)
        return K_HELP;
    if (ev & P_SEL) {                   /* SELECT and another button */
        if (ev & P_A)
            return K_OPTION;
        if (ev & P_B)
            snd_toggle();
        else if (ev & P_START)
            return K_ESC;
        return 0;
    }
    for (k = 0x80; !(ev & k); k >>= 1)  /* one button at a time: A first */
        ;
    ev = k;
    if (help_up)
        return ev == P_RIGHT ? '>' : ev == P_LEFT ? '<' : ev & (P_A | P_B | P_START) ? K_ESC : 0;
    if (!field)
        return title_key();
    return game_pad();
}
