/* Host tests for src/amiga/ui.c: what a click on each widget types, for each open prompt.
 *   make test-amiga-ui */
#include <stdio.h>
#include <string.h>
#include "../src/platform.h"
#include "../src/amiga/keys.h"
#include "../src/amiga/ui.h"
#include "../src/common/game.h"

/* the game state ui.c reads (game.c's, here set by hand) */
unsigned char board[NODES], hive_size[NSYN], coins[SEATS][NSYN], seat_kind[SEATS], me, turn, last_node, winners;
unsigned cash[SEATS];
char seat_name[SEATS][8];
const char syn_letter[NSYN + 1] = "GRHIBLO";
const char *const syn_name[NSYN];
unsigned char kit[RULE_KITSIZE], region[12], region_len, hints, link_up, status[30], refused;
unsigned char pr_kind, pr[18], step, edit[9], edit_len, leaving;
const char *complaint;

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; fixture(); printf("%s\n", #t); t(); } while (0)

static struct layout pal, ntsc;
static unsigned char keys[UI_KEYS_MAX];

static void fill_dialog(struct layout *l)
{
    int i;
    l->dlg.x = 46; l->dlg.y = 22; l->dlg.w = 228; l->dlg.h = 170;
    for (i = 0; i < NSYN; ++i) { l->dlg_row[i].x = 50; l->dlg_row[i].y = 60 + i * 15; l->dlg_row[i].w = 220; l->dlg_row[i].h = 14; }
    for (i = 0; i < UI_BTN_MAX; ++i) { l->btn[i].x = 50 + (i % 6) * 40; l->btn[i].y = 170 + (i / 6) * 4; l->btn[i].w = 36; l->btn[i].h = 3; }
}
static void fixture(void)
{
    int i;
    layout_make(&pal, 256);
    layout_make(&ntsc, 200);
    fill_dialog(&pal);
    fill_dialog(&ntsc);
    memset(board, 0, sizeof board);
    for (i = 0; i < RULE_KITSIZE; ++i) kit[i] = NONE;
    memset(pr, 0, sizeof pr);
    pr_kind = 0; step = 0; edit_len = 0; me = 2; turn = 2; link_up = 1; leaving = 0;
    ui_title_mode(0); ui_ask_yes_no(0); ui_reset(); ui_press_forget();
}
static void test_return_answers_yes_where_yes_is_lit(void)
{
    CHECK(ui_key(K_RETURN) == K_RETURN);                /* nothing asked: Return is Return */
    pr_kind = 'B'; step = 1;                            /* the end-the-game question */
    CHECK(ui_key(K_RETURN) == 'Y' && ui_key('N') == 'N' && ui_key(K_ESC) == K_ESC);
    step = 0; leaving = 1;                              /* log off */
    CHECK(ui_key(K_RETURN) == 'Y');
    leaving = 0; ui_title_mode(1); ui_ask_yes_no(1);    /* the title's quit */
    CHECK(ui_key(K_RETURN) == 'Y');
    ui_ask_yes_no(0);
    CHECK(ui_key(K_RETURN) == K_RETURN);                /* the title without a question: Return jacks in */
    pr_kind = 'B'; step = 0;                            /* the buy window: Return buys */
    ui_title_mode(0);
    CHECK(ui_key(K_RETURN) == K_RETURN);
}
static void centre(const struct rect *r, short *x, short *y) { *x = r->x + r->w / 2; *y = r->y + r->h / 2; }
static unsigned char click_rect(const struct layout *l, const struct rect *r, unsigned char button)
{
    short x, y; centre(r, &x, &y); return ui_click(l, x, y, button, keys);
}
static unsigned char click_node(const struct layout *l, unsigned char node)
{
    struct rect r; layout_node_rect(l, node, &r); return click_rect(l, &r, UI_LEFT);
}

static void test_node_in_kit_runs_that_exploit(void)
{
    pr_kind = 'P'; kit[3] = 40; kit[0] = 7;
    CHECK(click_node(&pal, 40) == 1 && keys[0] == '4');
    CHECK(click_node(&pal, 7) == 1 && keys[0] == '1');
}
static void test_node_not_in_kit_is_nothing(void)
{
    pr_kind = 'P'; kit[3] = 40;
    CHECK(click_node(&pal, 41) == 0);
}
static void test_node_with_blocked_exploit_is_nothing(void)
{
    pr_kind = 'P'; kit[1] = 55 | 0x80;                  /* bit 7: can't run now */
    CHECK(click_node(&pal, 55) == 0);
}
static void test_kit_chip_runs_that_exploit(void)
{
    pr_kind = 'P'; kit[4] = 12;
    CHECK(click_rect(&pal, &pal.kit_chip[4], UI_LEFT) == 1 && keys[0] == '5');
}
static void test_kit_chip_greyed_no_key(void)
{
    pr_kind = 'P'; kit[2] = 9 | 0x80;
    CHECK(click_rect(&pal, &pal.kit_chip[2], UI_LEFT) == 0);
    CHECK(click_rect(&pal, &pal.kit_chip[5], UI_LEFT) == 0);   /* empty slot */
}
static void test_infiltration_picks_a_syndicate(void)
{
    pr_kind = 'F'; pr[1] = 1 << 2 | 1 << 4;               /* H and B available */
    CHECK(click_rect(&pal, &pal.syn_row[2], UI_LEFT) == 1 && keys[0] == 'H');
    CHECK(click_rect(&pal, &pal.syn_row[4], UI_LEFT) == 1 && keys[0] == 'B');
    CHECK(click_rect(&pal, &pal.syn_row[3], UI_LEFT) == 0);   /* not offered */
}
static void test_infiltration_by_clicking_the_hive(void)
{
    pr_kind = 'F'; pr[1] = 1 << 5; board[30] = 2 + 5; board[31] = 2 + 1;
    CHECK(click_node(&pal, 30) == 1 && keys[0] == 'L');
    CHECK(click_node(&pal, 31) == 0);
}
static void test_tie_uses_the_options(void)
{
    pr_kind = 'K'; pr[1] = 0; pr[2] = 1 << 0 | 1 << 6;
    CHECK(click_rect(&pal, &pal.syn_row[6], UI_LEFT) == 1 && keys[0] == 'O');
    CHECK(click_rect(&pal, &pal.syn_row[1], UI_LEFT) == 0);
}
static void test_buy_click_adds_a_coin_of_that_syndicate(void)
{
    pr_kind = 'B'; pr[4 + 1] = 4; pr[4 + 5] = 3;
    CHECK(click_rect(&pal, &pal.dlg_row[1], UI_LEFT) == 1 && keys[0] == 'R');
    CHECK(click_rect(&pal, &pal.dlg_row[5], UI_LEFT) == 1 && keys[0] == 'L');
}
static void test_greyed_syndicate_no_key(void)
{
    pr_kind = 'B'; pr[4 + 1] = 4;                         /* only R is on the grid */
    CHECK(click_rect(&pal, &pal.dlg_row[0], UI_LEFT) == 0);
}
static void test_buy_and_undo_buttons(void)
{
    pr_kind = 'B';
    CHECK(click_rect(&pal, &pal.btn[BTN_BUY], UI_LEFT) == 1 && keys[0] == K_RETURN);
    CHECK(click_rect(&pal, &pal.btn[BTN_UNDO], UI_LEFT) == 1 && keys[0] == K_DELETE);
}
static void test_null_route_button_only_when_allowed(void)
{
    pr_kind = 'B'; pr[3] = 0;
    CHECK(click_rect(&pal, &pal.btn[BTN_NULLROUTE], UI_LEFT) == 0);
    pr[3] = 1;
    CHECK(click_rect(&pal, &pal.btn[BTN_NULLROUTE], UI_LEFT) == 1 && keys[0] == K_RETURN);   /* game.c then asks Y/N */
}
static void test_null_route_question_answers(void)
{
    pr_kind = 'B'; step = 1;
    CHECK(click_rect(&pal, &pal.btn[BTN_YES], UI_LEFT) == 1 && keys[0] == 'Y');
    CHECK(click_rect(&pal, &pal.btn[BTN_NO], UI_LEFT) == 1 && keys[0] == 'N');
    CHECK(click_rect(&pal, &pal.dlg_row[1], UI_LEFT) == 0);   /* a coin can't be added now */
}
static void test_right_undo_in_buy(void)
{
    pr_kind = 'B';
    CHECK(click_rect(&pal, &pal.dlg_row[1], UI_RIGHT) == 1 && keys[0] == K_DELETE);
    step = 1;
    CHECK(click_rect(&pal, &pal.dlg_row[1], UI_RIGHT) == 1 && keys[0] == 'N');
}
static void test_hivejack_count_digits(void)
{
    pr_kind = 'D'; pr[3] = 20;
    int i;
    for (i = 0; i < 12; ++i) CHECK(click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT) == 0);
    CHECK(ui_count() == 12 && ui_dirty() == 1 && ui_dirty() == 0);
    CHECK(click_rect(&pal, &pal.btn[BTN_OK], UI_LEFT) == 3);
    CHECK(keys[0] == '1' && keys[1] == '2' && keys[2] == K_RETURN);
}
static void test_hivejack_zero(void)
{
    pr_kind = 'D'; pr[3] = 20;
    CHECK(click_rect(&pal, &pal.btn[BTN_OK], UI_LEFT) == 2 && keys[0] == '0' && keys[1] == K_RETURN);
}
static void test_hivejack_bounds(void)
{
    int i;
    pr_kind = 'D'; pr[3] = 3;
    for (i = 0; i < 10; ++i) click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT);
    CHECK(ui_count() == 3);                                /* never more than held */
    for (i = 0; i < 10; ++i) click_rect(&pal, &pal.btn[BTN_MINUS], UI_LEFT);
    CHECK(ui_count() == 0);
}
static void test_hivejack_swap_steps_by_two(void)
{
    pr_kind = 'D'; pr[3] = 10; pr[4] = 50;
    click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT); click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT);   /* sell 2 */
    click_rect(&pal, &pal.btn[BTN_OK], UI_LEFT);
    step = 1;                                              /* game.c has moved on to the swap */
    click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT);
    CHECK(ui_count() == 2);                                /* even only */
    { int i; for (i = 0; i < 20; ++i) click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT); }
    CHECK(ui_count() == 8);                                /* 10 held, 2 sold: at most 8 */
}
static void test_hivejack_clears_typed_digits_first(void)
{
    pr_kind = 'D'; pr[3] = 20; edit_len = 2;
    click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT);
    CHECK(click_rect(&pal, &pal.btn[BTN_OK], UI_LEFT) == 4);
    CHECK(keys[0] == K_DELETE && keys[1] == K_DELETE && keys[2] == '1' && keys[3] == K_RETURN);
}
static void test_hivejack_right_cancels_count(void)
{
    pr_kind = 'D'; pr[3] = 20;
    click_rect(&pal, &pal.btn[BTN_PLUS], UI_LEFT);
    CHECK(click_rect(&pal, &pal.btn[BTN_OK], UI_RIGHT) == 0);
    CHECK(ui_count() == 0);
}
static void test_seat_count_chips(void)
{
    pr_kind = 'S';
    CHECK(click_rect(&pal, &pal.btn[BTN_SEAT2], UI_LEFT) == 1 && keys[0] == '2');
    CHECK(click_rect(&pal, &pal.btn[BTN_SEAT6], UI_LEFT) == 1 && keys[0] == '6');
}
static void test_title_buttons(void)
{
    ui_title_mode(1); pr_kind = 0; link_up = 0;
    edit_len = 3;
    CHECK(click_rect(&pal, &pal.btn[BTN_JACK], UI_LEFT) == 1 && keys[0] == K_RETURN);
    edit_len = 0;
    CHECK(click_rect(&pal, &pal.btn[BTN_JACK], UI_LEFT) == 0);     /* no handle yet: Return would do nothing */
    CHECK(click_rect(&pal, &pal.btn[BTN_HINTS], UI_LEFT) == 1 && keys[0] == K_OPTION);
    CHECK(click_rect(&pal, &pal.btn[BTN_SOUND], UI_LEFT) == 1 && keys[0] == AK_SOUND);
    CHECK(click_rect(&pal, &pal.btn[BTN_HELP], UI_LEFT) == 1 && keys[0] == K_HELP);
    CHECK(click_rect(&pal, &pal.btn[BTN_QUIT], UI_LEFT) == 1 && keys[0] == K_ESC);
    CHECK(click_rect(&pal, &pal.btn[BTN_UNDO], UI_RIGHT) == 1 && keys[0] == K_ESC);
}
static void test_yes_no_question(void)
{
    ui_title_mode(1); ui_ask_yes_no(1);
    CHECK(click_rect(&pal, &pal.btn[BTN_YES], UI_LEFT) == 1 && keys[0] == 'Y');
    CHECK(click_rect(&pal, &pal.btn[BTN_NO], UI_LEFT) == 1 && keys[0] == 'N');
    CHECK(ui_click(&pal, 3, 3, UI_LEFT, keys) == 1 && keys[0] == 'N');   /* anywhere else is a no, never a yes */
    CHECK(ui_click(&pal, 3, 3, UI_RIGHT, keys) == 1 && keys[0] == 'N');
}
static void test_log_off_question_in_play(void)
{
    leaving = 1; pr_kind = 'P'; kit[0] = 5;
    CHECK(click_rect(&pal, &pal.btn[BTN_YES], UI_LEFT) == 1 && keys[0] == 'Y');
    CHECK(click_node(&pal, 5) == 1 && keys[0] == 'N');         /* the playfield is not clickable meanwhile */
}
static void test_no_prompt_no_key(void)
{
    pr_kind = 0; kit[0] = 5;
    CHECK(click_node(&pal, 5) == 0);
    CHECK(click_rect(&pal, &pal.btn[BTN_BUY], UI_LEFT) == 0 || keys[0] == AK_TABLE);   /* the fixture's buttons lie over the seats */
    CHECK(click_rect(&pal, &pal.syn_row[1], UI_LEFT) == 0);
}
static void test_other_seats_turn_no_key(void)
{
    pr_kind = 'P'; kit[0] = 5; turn = 4; me = 2;
    CHECK(click_node(&pal, 5) == 0);
    CHECK(click_rect(&pal, &pal.kit_chip[0], UI_LEFT) == 0);
}
static void test_link_down_no_key(void)
{
    pr_kind = 'P'; kit[0] = 5; link_up = 0;
    CHECK(click_node(&pal, 5) == 0);
}
static void test_click_outside_everything(void)
{
    pr_kind = 'B'; pr[4 + 1] = 4;
    CHECK(ui_click(&pal, 0, 0, UI_LEFT, keys) == 0);
    CHECK(ui_click(&pal, -5, 300, UI_LEFT, keys) == 0);
    CHECK(ui_click(&pal, 32000, -32000, UI_LEFT, keys) == 0);
    CHECK(ui_click(&pal, 0, 0, 9, keys) == 0);                 /* a button that is neither */
}
static void test_same_answers_pal_and_ntsc(void)
{
    unsigned char a, b;
    pr_kind = 'P'; kit[1] = 77; kit[3] = 30;
    a = click_node(&pal, 77); b = click_node(&ntsc, 77);
    CHECK(a == 1 && b == 1);
    CHECK(click_rect(&pal, &pal.kit_chip[3], UI_LEFT) == 1 && keys[0] == '4');
    CHECK(click_rect(&ntsc, &ntsc.kit_chip[3], UI_LEFT) == 1 && keys[0] == '4');
    pr_kind = 'F'; pr[1] = 1 << 3;
    CHECK(click_rect(&ntsc, &ntsc.syn_row[3], UI_LEFT) == 1 && keys[0] == 'I');
}

static void test_seat_area_click_flips_the_view(void)
{
    struct rect *a = &pal.seats_area;
    pr_kind = 0;                                                /* in play, nothing asked */
    CHECK(click_rect(&pal, a, UI_LEFT) == 1 && keys[0] == AK_TABLE);
    CHECK(click_rect(&ntsc, &ntsc.seats_area, UI_LEFT) == 1 && keys[0] == AK_TABLE);
    pr_kind = 'P';
    CHECK(click_rect(&pal, a, UI_LEFT) == 1 && keys[0] == AK_TABLE);   /* my move, a gap in the seats */
    CHECK(click_rect(&pal, a, UI_RIGHT) == 0);
}
static void test_seat_area_belongs_to_a_window_when_one_is_up(void)
{
    pr_kind = 'S';
    CHECK(click_rect(&pal, &pal.seats_area, UI_LEFT) != 1 || keys[0] != AK_TABLE);
    pr_kind = 'B';
    CHECK(click_rect(&pal, &pal.seats_area, UI_LEFT) != 1 || keys[0] != AK_TABLE);
    leaving = 1; pr_kind = 0;
    CHECK(click_rect(&pal, &pal.seats_area, UI_LEFT) != 1 || keys[0] != AK_TABLE);
    leaving = 0;
}
static void test_help_clicks_turn_pages_and_leave(void)
{
    ui_help_mode(1);
    CHECK(click_rect(&pal, &pal.help_next, UI_LEFT) == 1 && keys[0] == '>');
    CHECK(click_rect(&pal, &pal.help_prev, UI_LEFT) == 1 && keys[0] == '<');
    CHECK(click_rect(&ntsc, &ntsc.help_exit, UI_LEFT) == 1 && keys[0] == K_ESC);
    CHECK(click_rect(&pal, &pal.help_page, UI_RIGHT) == 1 && keys[0] == K_ESC);
    CHECK(click_node(&pal, 40) == 0);                          /* the playfield under it is not clickable */
    ui_help_mode(0);
    CHECK(click_rect(&pal, &pal.help_next, UI_LEFT) == 0 || (keys[0] != '>' && keys[0] != K_ESC));
}
static void test_double_click_is_one_press(void)
{
    CHECK(ui_press_ok(100, 0, 32) == 1);
    ui_press_taken(100);
    CHECK(ui_press_ok(101, 0, 32) == 0 && ui_press_ok(102, 0, 32) == 0);   /* the second of a double click */
    CHECK(ui_press_ok(103, 0, 32) == 1);                                   /* a deliberate second press */
}
static void test_press_gap_survives_the_tick_wrapping(void)
{
    ui_press_taken(254);
    CHECK(ui_press_ok(255, 0, 32) == 0 && ui_press_ok(0, 0, 32) == 0);
    CHECK(ui_press_ok(1, 0, 32) == 1);
}
static void test_press_gap_is_forgotten_when_the_tick_laps(void)
{
    ui_press_taken(10);
    ui_press_age(14);                                                      /* the loop saw the gap pass */
    CHECK(ui_press_ok((unsigned char)(10 + 256 + 1), 0, 32) == 1);         /* 256 ticks on, a press is not "1 tick" after */
}
static void test_ok_after_typing_sends_what_was_typed(void)
{
    pr_kind = 'D'; pr[3] = 20; edit_len = 1;
    CHECK(click_rect(&pal, &pal.btn[BTN_OK], UI_LEFT) == 1 && keys[0] == K_RETURN);   /* the stepper was never moved */
}
static void test_hammering_is_dropped_when_the_ring_is_half_full(void)
{
    CHECK(ui_press_ok(50, 15, 32) == 1);
    CHECK(ui_press_ok(50, 16, 32) == 0);                                   /* half full: no more keys queued */
    CHECK(ui_press_ok(50, 31, 32) == 0);
}
static void test_a_press_that_made_no_key_does_not_start_the_gap(void)
{
    CHECK(ui_press_ok(10, 0, 32) == 1);                                    /* nothing was taken: not recorded */
    CHECK(ui_press_ok(10, 0, 32) == 1);
}

int main(void)
{
    RUN(test_node_in_kit_runs_that_exploit); RUN(test_node_not_in_kit_is_nothing);
    RUN(test_node_with_blocked_exploit_is_nothing); RUN(test_kit_chip_runs_that_exploit);
    RUN(test_kit_chip_greyed_no_key); RUN(test_infiltration_picks_a_syndicate);
    RUN(test_infiltration_by_clicking_the_hive); RUN(test_tie_uses_the_options);
    RUN(test_buy_click_adds_a_coin_of_that_syndicate); RUN(test_greyed_syndicate_no_key);
    RUN(test_buy_and_undo_buttons); RUN(test_null_route_button_only_when_allowed);
    RUN(test_null_route_question_answers); RUN(test_right_undo_in_buy);
    RUN(test_hivejack_count_digits); RUN(test_hivejack_zero); RUN(test_hivejack_bounds);
    RUN(test_hivejack_swap_steps_by_two); RUN(test_hivejack_clears_typed_digits_first);
    RUN(test_hivejack_right_cancels_count); RUN(test_seat_count_chips); RUN(test_title_buttons);
    RUN(test_yes_no_question); RUN(test_log_off_question_in_play); RUN(test_no_prompt_no_key);
    RUN(test_other_seats_turn_no_key); RUN(test_link_down_no_key); RUN(test_click_outside_everything);
    RUN(test_same_answers_pal_and_ntsc); RUN(test_seat_area_click_flips_the_view); RUN(test_seat_area_belongs_to_a_window_when_one_is_up); RUN(test_help_clicks_turn_pages_and_leave); RUN(test_double_click_is_one_press);
    RUN(test_press_gap_survives_the_tick_wrapping); RUN(test_press_gap_is_forgotten_when_the_tick_laps); RUN(test_ok_after_typing_sends_what_was_typed); RUN(test_hammering_is_dropped_when_the_ring_is_half_full);
    RUN(test_a_press_that_made_no_key_does_not_start_the_gap); RUN(test_return_answers_yes_where_yes_is_lit);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
