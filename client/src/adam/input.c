/* The keyboard, the controller and the clock for the Adam. The game asks for
 * keys (platform.h's in_key). The keyboard types them as on the other
 * computers; the controller works as the NES's pad (src/nes/input.c, whose
 * cursor and dial this is), its presses latched by the NMI (nmi.asm) in the
 * NES's own buttons:
 *
 *  keyboard   letters, digits, RETURN, BACKSPACE (Delete), ESCAPE (Esc);
 *             SmartKey I the instructions, II move hints, III sound; the
 *             arrows move the cursor as the stick does, and turn the pages
 *  left fire  A: the answer the cursor is on: the exploit's 1-6, the
 *             syndicate's letter, the seat count; YES at a yes/no question
 *  right fire B: Delete (a bought letter, the handle's letter); NO at a yes/no
 *  #          START: Return: join, buy, a count confirmed
 *  stick      moves the cursor: an exploit in the kit, an offered syndicate, a
 *             syndicate in the hives panel while buying; up and down change a
 *             count (a hivejack's cash-out and swap, the host's seats)
 *  *          SELECT: the instructions (on its own, as it is let go); in them
 *             the stick turns the pages, and either fire or # goes back;
 *             *+left fire move hints, *+right fire sound, *+# Esc (log off)
 *  0-9        the keypad's digits type
 *
 * The cursor is `pick` (the display draws it, display_pick); it goes to the
 * first answer that can be taken when a prompt opens: display_prompt asks
 * (pick_check), so a prompt's first picture has it in place. A count is typed for the
 * game as edit[] holds it: it is written there, as typing would, and read back
 * from there, so the keyboard and the stick can both change it. On the title
 * the stick is a dial: up and down step the letter it is on (A-Z, 0-9, then the
 * signs a handle can have), left and right move along the handle, right fire
 * deletes the letter; a letter typed puts it at the end. All of it is in this
 * file; the common code sees only keys.
 *
 * ticks() is the NMI's frame count, 60 a second. */
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

#define K_UP     0xA0                   /* the Adam's keyboard */
#define K_RIGHT  0xA1
#define K_DOWN   0xA2
#define K_LEFT   0xA3
#define K_SMART1 0x81
#define K_SMART2 0x82
#define K_SMART3 0x83

extern volatile unsigned char pad_ring[8], pad_head, pad_tail, pad_now, pad_digit;  /* nmi.asm */
extern unsigned char field;             /* display_text.c: the playfield is up */
void display_pick(void);                /* display_text.c */
void eos_kbd_start(void);               /* eoscall.asm */
unsigned char eos_kbd_end(void);

unsigned char pick;                     /* the answer the cursor is on */
unsigned char dial = 0xFF;              /* the title: the handle's letter the dial is on; at first past the end, where typing goes */

static unsigned char kind, kstep, count, k, lim;

#ifndef PC_ADAM                         /* make pc-adam links the cursor only (test/adam_text_pc.c) */
static unsigned char ev, rep, rep_t, sold, asked;

unsigned char ticks(void)
{
    return frames;
}

unsigned char ticks_per_sec(void)
{
    return 60;
}

/* A character from the keyboard, if one came; the next is asked for at once. */
static unsigned char typed(void)
{
    if (!asked) {
        asked = 1;
        eos_kbd_start();
    }
    k = eos_kbd_end();
    if (!k)
        return 0;                       /* still waiting */
    eos_kbd_start();
    if (k == 1)
        return 0;                       /* it failed: asked again */
    if (k >= 'a' && k <= 'z')
        k -= 'a' - 'A';
    return k;
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
#endif

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
static void step_pick(unsigned char up)
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
}

/* The display, as it draws the prompt: if one opened (or moved on a step),
 * the cursor to its first answer, the count to 0 (shown as 0 while nothing is
 * typed). It draws nothing itself. */
void pick_check(void)
{
    if (pr_kind == kind && step == kstep)
        return;
    kind = pr_kind;
    kstep = step;
    switch (kind) {
    case 'S':                           /* the humans waiting, at least the fewest seats */
        for (pick = k = 0; k < SEATS; ++k)
            pick += seat_kind[k] == SEAT_HUMAN || seat_kind[k] == SEAT_AWAY;
        if (pick < RULE_MINPLAYERS)
            pick = RULE_MINPLAYERS;
        return;
    case 'D':
        count = 0;
        return;
    case 'P':                           /* the first exploit that can run, else the first there */
        for (pick = 0; pick < RULE_KITSIZE; ++pick)
            if (kit[pick] != NONE && !(kit[pick] & 0x80))
                return;
        /* fall through */
    default:
        pick = (kind == 'P' ? RULE_KITSIZE : NSYN) - 1;
        step_pick(1);
    }
}

#ifndef PC_ADAM
static void move(unsigned char up)
{
    step_pick(up);
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


/* The count as edit[] holds it: what the stick or the keyboard last made it. */
static void get_count(void)
{
    for (count = k = 0; k < edit_len; ++k)
        count = count * 10 + edit[k] - '0';
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
                get_count();
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
                get_count();
                k = step ? 2 : 1;
                count = count > k ? count - k : 0;
                set_count();
            }
            return 0;
        }
        if (ev & (P_A | P_START)) {
            if (kind == 'S')
                return '0' + pick;
            if (!step) {
                get_count();
                sold = count;
            }
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

/* A key from the keyboard, as the game takes it; the dial follows what is typed. */
static unsigned char key(void)
{
    switch (k) {
    case K_SMART1:
        return K_HELP;
    case K_SMART2:
        return K_OPTION;
    case K_SMART3:
        snd_toggle();
        return 0;
    }
    if (kind == 'D' && k == K_RETURN && !step) {
        get_count();                    /* a cash-out typed: what the swap can take */
        sold = count;
    }
    if (!field && !help_up) {
        if (k == K_DELETE)
            dial = edit_len ? edit_len - 1 : 0;
        else if (k > ' ' && k <= 'Z')
            dial = edit_len + 1;        /* past the letter it adds */
    }
    return k;
}

unsigned char in_key(void)
{
    if (typed()) {
        switch (k) {                    /* the arrows: as the stick */
        case K_UP:    ev = P_UP;    break;
        case K_DOWN:  ev = P_DOWN;  break;
        case K_LEFT:  ev = P_LEFT;  break;
        case K_RIGHT: ev = P_RIGHT; break;
        default:
            return key();
        }
    } else if (pad_digit) {
        k = pad_digit;
        pad_digit = 0;
        return key();
    } else {
        if (!next())
            return 0;
        if (ev == P_SEL)
            return K_HELP;
        if (ev & P_SEL) {               /* SELECT and another button */
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
    }
    if (help_up)
        return ev == P_RIGHT ? '>' : ev == P_LEFT ? '<' : ev & (P_A | P_B | P_START) ? K_ESC : 0;
    if (!field)
        return title_key();
    return game_pad();
}
#endif
