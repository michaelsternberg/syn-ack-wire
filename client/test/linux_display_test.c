/* Host tests for src/linux/display_ansi.c: the game state goes in, the cell array is read back as text and colours.
 *   make test-linux-display */
#include <stdio.h>
#include <string.h>
#include "../src/config.h"
#include "../src/platform.h"
#include "../src/display.h"
#include "../src/common/proto.h"
#include "../src/common/proto_tab.h"
#include "../src/common/game.h"
#include "../src/linux/screen.h"
#include "../src/linux/layout.h"

/* ---- the game state, as game.c would hold it */
const char syn_letter[NSYN + 1] = "GRHIBLO";
const char *const syn_name[NSYN] = { "GHOST//MODEM", "RAT KING CARTEL", "HOLY DAEMON", "IRON VECTOR", "BRATVA 404",
                                     "LOTUS-12 TRIAD", "OBSIDIAN LEDGER" };
unsigned char board[NODES], hive_size[NSYN], seat_kind[SEATS], coins[SEATS][NSYN];
unsigned cash[SEATS];
char seat_name[SEATS][8];
unsigned char me, turn = NONE, last_node = NONE, winners, kit[RULE_KITSIZE], region[12], region_len, hints, link_up = 1;
unsigned char status[30], refused, pr_kind, pr[18], step, edit[9], edit_len, leaving;
const char *complaint;
unsigned char msg[128];
unsigned char log_count, log_back;
static unsigned char logbuf[8][40];
const unsigned char *log_line(unsigned char n) { return n < log_count ? logbuf[(log_count - 1 - n) % 8] : 0; }
static void push_log(const char *t)
{
    unsigned char *d = logbuf[log_count % 8];
    d[0] = (unsigned char)strlen(t);
    memcpy(d + 1, t, d[0]);
    ++log_count;
}
static unsigned char now;
unsigned char ticks(void) { return now; }
unsigned char ticks_per_sec(void) { return 50; }

/* ---- reading the cells back */
static char rowbuf[NL_COLS + 1];
static const char *row_text(unsigned char r)
{
    unsigned char c;
    for (c = 0; c < NL_COLS; ++c)
        rowbuf[c] = (char)nl_ascii(nl_cells[r][c].ch);
    rowbuf[NL_COLS] = 0;
    return rowbuf;
}
static int at_text(unsigned char r, unsigned char c, const char *s)
{
    return strncmp(row_text(r) + c, s, strlen(s)) == 0;
}
static int blank_between(unsigned char r, unsigned char c0, unsigned char c1)
{
    for (; c0 < c1; ++c0)
        if (nl_cells[r][c0].ch != ' ')
            return 0;
    return 1;
}
#define CELL(r, c) nl_cells[r][c]
#define NODE(col, row) ((row) * RULE_COLS + (col))

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); reset(); t(); } while (0)

static void reset(void)
{
    unsigned char i;
    memset(board, 0, sizeof board); memset(hive_size, 0, sizeof hive_size); memset(coins, 0, sizeof coins);
    memset(cash, 0, sizeof cash); memset(seat_kind, 0, sizeof seat_kind); memset(seat_name, ' ', sizeof seat_name);
    for (i = 0; i < RULE_KITSIZE; ++i) kit[i] = NONE;
    me = 0; turn = NONE; last_node = NONE; hints = 0; link_up = 1; region_len = 0; winners = 0;
    pr_kind = 0; step = 0; edit_len = 0; complaint = 0; leaving = 0; log_count = log_back = 0; now = 0;
    nl_clear(C_WHITE, C_BG);
}
static void seat(unsigned char s, unsigned char kind, const char *name, unsigned c)
{
    seat_kind[s] = kind;
    memset(seat_name[s], ' ', 8);
    memcpy(seat_name[s], name, strlen(name));
    cash[s] = c;
}

static void test_title_bar(void)
{
    display_playfield();
    CHECK(at_text(ROW_BAR, 1, "SYN-ACK WIRE"));
    CHECK(CELL(ROW_BAR, 1).fg == C_CYAN && CELL(ROW_BAR, 9).fg == C_MAGENTA && CELL(ROW_BAR, 1).bg == C_BAR);
    CHECK(at_text(ROW_BAR, 17, "NEO-KANSAI-1:CONNECTED"));
    CHECK(CELL(ROW_BAR, 30).fg == C_GREEN);
    CHECK(at_text(ROW_BAR, 49, "? HELP") && at_text(ROW_BAR, 57, "TAB HINTS") && at_text(ROW_BAR, 68, "^S SOUND"));
    CHECK(CELL(ROW_BAR, 79).bg == C_BAR);
}
static void test_region_and_link_from_the_server(void)
{
    memcpy(region, "CHIBA-GRID-7", 12); region_len = 12; link_up = 0;
    display_playfield();
    CHECK(at_text(ROW_BAR, 17, "CHIBA-GRID-7:NO SIGNAL"));
    CHECK(CELL(ROW_BAR, 30).fg == C_RED);
    link_up = 1; display_link();
    CHECK(at_text(ROW_BAR, 17, "CHIBA-GRID-7:CONNECTED"));
    CHECK(blank_between(ROW_BAR, 39, 48));          /* no NO SIGNAL left over */
}
static void test_grid_headers_and_cells(void)
{
    board[NODE(0, 0)] = 2 + 6;                      /* A0: O, lavender */
    board[NODE(1, 0)] = CELL_SPIKE;
    display_playfield();
    CHECK(at_text(ROW_HEAD, COL_GRID + 1, "A") && at_text(ROW_HEAD, COL_GRID + 3 * 11 + 1, "L"));
    CHECK(at_text(ROW_GRID + 8, COL_ROWNUM, "8"));
    CHECK(at_text(ROW_GRID, COL_GRID, " O ") && CELL(ROW_GRID, COL_GRID + 1).bg == C_SYN0 + 6 && CELL(ROW_GRID, COL_GRID + 1).fg == C_BLACK);
    CHECK(CELL(ROW_GRID, COL_GRID).bg == C_SYN0 + 6 && CELL(ROW_GRID, COL_GRID + 2).bg == C_SYN0 + 6);
    CHECK(at_text(ROW_GRID, COL_GRID + 3, " + ") && CELL(ROW_GRID, COL_GRID + 4).bg == C_GREY);
    CHECK(at_text(ROW_GRID, COL_GRID + 6, " . ") && CELL(ROW_GRID, COL_GRID + 7).bg == C_EMPTY && CELL(ROW_GRID, COL_GRID + 7).fg == C_DIM);
}
static void test_hint_digits_only_on_empty_runnable_nodes(void)
{
    kit[0] = NODE(2, 3); kit[1] = NODE(4, 4) | 0x80; kit[2] = NODE(5, 5);
    board[NODE(5, 5)] = 2;                          /* taken since: no hint on a hive */
    display_playfield();
    CHECK(at_text(ROW_GRID + 3, COL_GRID + 6, " . "));       /* hints off */
    hints = 1; display_playfield();
    CHECK(at_text(ROW_GRID + 3, COL_GRID + 6, " 1 ") && CELL(ROW_GRID + 3, COL_GRID + 7).fg == C_YELLOW);
    CHECK(at_text(ROW_GRID + 4, COL_GRID + 12, " . "));      /* bit 7: can't run now */
    CHECK(at_text(ROW_GRID + 5, COL_GRID + 15, " G "));
    hints = 0; display_node(NODE(2, 3));
    CHECK(at_text(ROW_GRID + 3, COL_GRID + 6, " . "));       /* display_node follows the flag */
}
static void test_syndicate_panel_sizes_bars_and_ice(void)
{
    hive_size[0] = 28; hive_size[4] = 27; hive_size[6] = 11; hive_size[2] = 10; hive_size[1] = 41;
    display_playfield();
    CHECK(at_text(ROW_HEAD, COL_HIVE, "HIVES") && CELL(ROW_HEAD, COL_HIVE).fg == C_CYAN);
    CHECK(at_text(ROW_HEAD, COL_BAR, "SIZE") && at_text(ROW_HEAD, COL_NAME, "SYNDICATE"));
    CHECK(at_text(ROW_GRID, COL_HIVE, " G ") && CELL(ROW_GRID, COL_HIVE).bg == C_SYN0);
    CHECK(at_text(ROW_GRID, COL_SIZE, " 28"));
    CHECK(CELL(ROW_GRID, COL_BAR + 4).bg == C_SYN0 && CELL(ROW_GRID, COL_BAR + 5).bg == C_EMPTY);   /* 28*8/41 = 5 */
    CHECK(at_text(ROW_GRID, COL_NAME, "GHOST//MODEM") && CELL(ROW_GRID, COL_NAME).fg == C_SYN0);
    CHECK(at_text(ROW_GRID, COL_ICE, "ICE") && CELL(ROW_GRID, COL_ICE).bg == C_BLUE);
    CHECK(at_text(ROW_GRID + 1, COL_SIZE, " 41") && CELL(ROW_GRID + 1, COL_BAR + 7).bg == C_SYN0 + 1);   /* a full bar */
    CHECK(at_text(ROW_GRID + 2, COL_SIZE, " 10") && !at_text(ROW_GRID + 2, COL_ICE, "ICE"));           /* 10: not safe yet */
    CHECK(at_text(ROW_GRID + 6, COL_SIZE, " 11") && at_text(ROW_GRID + 6, COL_ICE, "ICE"));            /* 11: safe */
    CHECK(at_text(ROW_GRID + 3, COL_SIZE, "  -") && CELL(ROW_GRID + 3, COL_SIZE + 2).fg == C_DIM);
    CHECK(CELL(ROW_GRID + 3, COL_BAR).bg == C_EMPTY && !at_text(ROW_GRID + 3, COL_ICE, "ICE"));        /* no hive: no bar */
    CHECK(CELL(ROW_GRID, COL_NAME + 14).ch == ' ');                                                   /* a short name pads out */
}
static void test_a_hive_of_one_still_shows_a_bar(void)
{
    hive_size[3] = 2;
    display_playfield();
    CHECK(CELL(ROW_GRID + 3, COL_BAR).bg == C_SYN0 + 3);
}
static void test_safe_and_end_line(void)
{
    display_playfield();
    CHECK(at_text(ROW_ICE, COL_HIVE, "BLACK ICE AT 11, NULL-ROUTE AT 41"));
    CHECK(CELL(ROW_ICE, COL_HIVE + 13).fg == C_WHITE && CELL(ROW_ICE, COL_HIVE).fg == C_DIM);
}
static void test_last_move(void)
{
    display_playfield();
    CHECK(blank_between(ROW_LAST, 0, NL_COLS));
    last_node = NODE(10, 6);
    display_node(last_node);
    CHECK(at_text(ROW_LAST, COL_GRID, "LAST MOVE K6") && CELL(ROW_LAST, COL_GRID + 10).fg == C_WHITE);
}
static void test_seats(void)
{
    seat(0, SEAT_HUMAN, "ANDY-9", 642);
    seat(1, SEAT_BOT, "WIREWOLF", 315);
    seat(2, SEAT_AWAY, "SEGFAULT", 65535);
    coins[0][2] = 7; coins[0][6] = 12; me = 0; turn = 1;
    display_playfield();
    CHECK(at_text(ROW_SEATHEAD, 3, "NETRUNNERS") && at_text(ROW_SEATHEAD, 18, "ACCT"));
    CHECK(at_text(ROW_SEATHEAD, COL_HOLD0, " G ") && CELL(ROW_SEATHEAD, COL_HOLD0 + 1).bg == C_SYN0
          && CELL(ROW_SEATHEAD, COL_HOLD0 + 6 * HOLD_STEP + 1).bg == C_SYN0 + 6);
    CHECK(at_text(ROW_SEATS, COL_SEAT, "ANDY-9") && at_text(ROW_SEATS, COL_WHO, "YOU"));
    CHECK(at_text(ROW_SEATS, COL_TURN, " ") && at_text(ROW_SEATS + 1, COL_TURN, ">"));              /* the marker is on the seat to move */
    CHECK(at_text(ROW_SEATS, COL_CASH, " Y64,200") && CELL(ROW_SEATS, COL_CASH + 1).fg == C_YELLOW);
    CHECK(at_text(ROW_SEATS + 1, COL_SEAT, "WIREWOLF*") && at_text(ROW_SEATS + 1, COL_WHO, "BOT"));
    CHECK(at_text(ROW_SEATS + 2, COL_SEAT, "SEGFAULT?"));
    CHECK(at_text(ROW_SEATS + 2, COL_CASH, "Y6553500"));                                            /* the widest cash fills its field, no more */
    CHECK(CELL(ROW_SEATS + 2, COL_CASH - 1).ch == ' ' && CELL(ROW_SEATS + 2, COL_CASH + CASH_W).ch == ' ');
    CHECK(at_text(ROW_SEATS, COL_HOLD0 + 2, ".") && at_text(ROW_SEATS, COL_HOLD0 + 2 * HOLD_STEP + 2, "7"));
    CHECK(at_text(ROW_SEATS, COL_HOLD0 + 6 * HOLD_STEP + 1, "12") && CELL(ROW_SEATS, COL_HOLD0 + 6 * HOLD_STEP + 1).fg == C_WHITE);
    CHECK(CELL(ROW_SEATS, COL_HOLD0 + 2).fg == C_DIM);
    CHECK(at_text(ROW_SEATS + 3, COL_SEAT, "-") && CELL(ROW_SEATS + 3, COL_SEAT).fg == C_DIM && blank_between(ROW_SEATS + 3, 4, NL_COLS));
}
static void test_display_seat_changes_only_its_row(void)
{
    nl_cell before[NL_ROWS][NL_COLS];
    seat(0, SEAT_HUMAN, "ANDY-9", 642);
    display_playfield();
    memcpy(before, nl_cells, sizeof before);
    cash[0] = 100;
    display_seat(0);
    CHECK(memcmp(before[ROW_SEATS - 1], nl_cells[ROW_SEATS - 1], sizeof before[0] * 1) == 0);
    CHECK(memcmp(before[ROW_SEATS + 1], nl_cells[ROW_SEATS + 1], sizeof before[0] * (NL_ROWS - ROW_SEATS - 1)) == 0);
    CHECK(at_text(ROW_SEATS, COL_CASH, " Y10,000"));
}
static void test_display_node_changes_only_its_cells(void)
{
    nl_cell before[NL_ROWS][NL_COLS];
    unsigned char r, c, changed = 0;
    display_playfield();
    memcpy(before, nl_cells, sizeof before);
    board[NODE(3, 4)] = 2 + 4;
    display_node(NODE(3, 4));
    for (r = 0; r < NL_ROWS; ++r)
        for (c = 0; c < NL_COLS; ++c)
            if (memcmp(&before[r][c], &nl_cells[r][c], sizeof before[0][0])) {
                ++changed;
                CHECK(r == ROW_GRID + 4 && c >= COL_GRID + 9 && c < COL_GRID + 12);
            }
    CHECK(changed == 3);
}
static void test_the_last_move_flashes_then_settles(void)
{
    board[NODE(0, 0)] = 2;
    last_node = NODE(0, 0);
    display_playfield();
    CHECK(CELL(ROW_GRID, COL_GRID + 1).bg == C_SYN0);
    now = 20; display_idle();                       /* lit */
    CHECK(CELL(ROW_GRID, COL_GRID + 1).bg == C_WHITE && CELL(ROW_GRID, COL_GRID + 1).fg == C_BLACK);
    now = 40; display_idle();                       /* unlit */
    CHECK(CELL(ROW_GRID, COL_GRID + 1).bg == C_SYN0);
    now = 255; display_idle();                      /* five seconds on: it stays as the hive */
    CHECK(CELL(ROW_GRID, COL_GRID + 1).bg == C_SYN0);
}
static void test_log_scrolls_and_marks(void)
{
    const char *lines[] = { "[$] WIREWOLF FINAL 31500", "[!] TOP NETRUNNER: ANDY-9", "SPIKE AT K6", "NEXT LINE" };
    unsigned char i;
    display_playfield();
    for (i = 0; i < 4; ++i) {
        msg[0] = 'l'; msg[1] = (unsigned char)strlen(lines[i]); memcpy(msg + 2, lines[i], msg[1]);
        display_log();
    }
    CHECK(at_text(ROW_LOG, COL_LOGMARK, "[!]") && CELL(ROW_LOG, COL_LOGMARK + 1).fg == C_MAGENTA);   /* the first scrolled off */
    CHECK(at_text(ROW_LOG, COL_LOGTEXT, "TOP NETRUNNER: ANDY-9"));
    CHECK(at_text(ROW_LOG + 1, COL_LOGTEXT, "SPIKE AT K6"));           /* a line with no marker is aligned with the others */
    CHECK(at_text(ROW_LOG + 2, COL_LOGTEXT, "NEXT LINE"));
    CHECK(blank_between(ROW_LOG + 2, COL_LOGTEXT + 9, NL_COLS) && blank_between(ROW_LOG + 2, 0, COL_LOGTEXT));
    msg[1] = 3; memcpy(msg + 2, "[$]", 3); display_log();
    CHECK(CELL(ROW_LOG + 2, COL_LOGMARK + 1).fg == C_GREEN);
}

/* ---- prompts */
static void ask_is(const char *s) { CHECK(at_text(ROW_ASK, COL_PROMPT, s)); }
static void strip_is_purple(void)
{
    unsigned char r, c;
    for (r = ROW_INFO; r <= ROW_ASK; ++r)
        for (c = 0; c < NL_COLS; ++c)
            if (nl_cells[r][c].bg != C_STRIP && nl_cells[r][c].bg < C_SYN0)
                { CHECK(nl_cells[r][c].bg == C_STRIP); return; }
}
static void test_waiting_for_another_seat_shows_the_kit(void)
{
    seat(0, SEAT_HUMAN, "ANDY-9", 642); seat(1, SEAT_BOT, "WIREWOLF", 315);
    me = 0; turn = 1; kit[0] = NODE(10, 6); kit[2] = NODE(0, 0) | 0x80;
    display_playfield();
    ask_is("WAITING FOR WIREWOLF");
    CHECK(blank_between(ROW_ASK, 21, NL_COLS));
    CHECK(at_text(ROW_INFO, COL_PROMPT, "EXPLOITS 1:K6 2:-- 3:A0"));
    CHECK(CELL(ROW_INFO, COL_PROMPT + 9).fg == C_YELLOW && CELL(ROW_INFO, COL_PROMPT + 11).fg == C_WHITE);
    CHECK(CELL(ROW_INFO, COL_PROMPT + 21).fg == C_DIM);               /* 3: A0 can't run now */
    strip_is_purple();
}
static void test_in_the_lobby_the_strip_is_empty(void)
{
    display_playfield();
    CHECK(blank_between(ROW_INFO, 0, NL_COLS) && blank_between(ROW_ASK, 0, NL_COLS));
    strip_is_purple();
}
static void test_start_prompt(void)
{
    pr_kind = 'S'; display_prompt();
    ask_is("START WITH HOW MANY SEATS? 2-6");
    CHECK(blank_between(ROW_INFO, 0, NL_COLS));
}
static void test_exploit_prompt(void)
{
    kit[1] = NODE(2, 3);
    pr_kind = 'P'; display_prompt();
    ask_is("RUN WHICH EXPLOIT? PRESS 1-6");
    CHECK(at_text(ROW_INFO, COL_PROMPT, "EXPLOITS 1:-- 2:C3"));
}
static void test_broker_prompt_names_the_syndicates_by_colour(void)
{
    pr_kind = 'F'; pr[1] = 1 << 0 | 1 << 2; display_prompt();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "INFILTRATION! BROKER TO "));
    CHECK(at_text(ROW_INFO, COL_PROMPT + 24, " G ") && CELL(ROW_INFO, COL_PROMPT + 25).bg == C_SYN0);
    CHECK(at_text(ROW_INFO, COL_PROMPT + 28, " H ") && CELL(ROW_INFO, COL_PROMPT + 29).bg == C_SYN0 + 2);
    CHECK(blank_between(ROW_INFO, COL_PROMPT + 31, NL_COLS));
    ask_is("PRESS A SYNDICATE LETTER");
}
static void test_tie_prompt_both_questions(void)
{
    pr_kind = 'K'; pr[1] = 0; pr[2] = 1 << 1 | 1 << 3; display_prompt();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "TIE! WHICH SURVIVES? ") && at_text(ROW_INFO, COL_PROMPT + 21, " R ") && at_text(ROW_INFO, COL_PROMPT + 25, " I "));
    pr[1] = 1; display_prompt();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "TIE! ABSORB WHICH NEXT? "));
    ask_is("PRESS A SYNDICATE LETTER");
}
static void test_hivejack_prompt_cash_out_then_swap(void)
{
    pr_kind = 'D'; pr[1] = 1; pr[2] = 0; pr[3] = 7; edit[0] = '3'; edit_len = 1;
    display_prompt();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "HIVEJACK: G TAKES R. YOU HOLD 7 R"));
    ask_is("CASH OUT HOW MANY? 3");
    step = 1; edit_len = 0; display_prompt();
    ask_is("SWAP 2 FOR 1, HOW MANY? ");
    CHECK(blank_between(ROW_ASK, 24, NL_COLS));
}
static void test_buy_prompt_and_the_buy_panel(void)
{
    hive_size[0] = 28; pr[4] = 12; pr[11] = 25; pr[5] = 0;
    pr_kind = 'B'; edit[0] = 'G'; edit[1] = 'G'; edit_len = 2;
    display_playfield();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "BUY UP TO 3 CRYPTO: GG"));
    ask_is("LETTERS, BACKSPACE, ENTER TO BUY");
    CHECK(at_text(ROW_HEAD, COL_BAR, "PRICE") && at_text(ROW_HEAD, COL_ICE - 1, "LEFT"));
    CHECK(at_text(ROW_GRID, COL_BAR + 2, "Y1,200") && CELL(ROW_GRID, COL_BAR + 3).fg == C_YELLOW);
    CHECK(at_text(ROW_GRID, COL_ICE + 1, "25") && !at_text(ROW_GRID, COL_ICE, "ICE"));
    CHECK(at_text(ROW_GRID, COL_SIZE, " 28"));                          /* the sizes stay */
    CHECK(at_text(ROW_GRID + 1, COL_NAME, "RAT KING CARTEL") && blank_between(ROW_GRID + 1, COL_BAR, COL_NAME - 1));  /* no price, nothing there */
    step = 1;
    display_prompt();
    ask_is("NULL-ROUTE AND END THE GAME? Y/N");
    pr_kind = 0; display_hives();
    CHECK(at_text(ROW_HEAD, COL_BAR, "SIZE") && blank_between(ROW_HEAD, COL_ICE - 1, NL_COLS));
    CHECK(!at_text(ROW_GRID, COL_ICE + 1, "25") && at_text(ROW_GRID, COL_ICE, "ICE"));
}
static void test_buy_step_two_with_nothing_bought_shows_only_the_question(void)
{
    pr_kind = 'B'; step = 1; edit_len = 0;
    display_prompt();
    CHECK(blank_between(ROW_INFO, 0, NL_COLS));
    ask_is("NULL-ROUTE AND END THE GAME? Y/N");
}
static void test_a_complaint_replaces_the_question(void)
{
    pr_kind = 'S'; complaint = "[-] NOT ENOUGH CASH";
    display_prompt();
    ask_is("[-] NOT ENOUGH CASH");
    CHECK(blank_between(ROW_ASK, 20, NL_COLS));
}
static void test_logging_off_asks_then_the_prompt_comes_back(void)
{
    pr_kind = 'S'; leaving = 1; display_prompt();
    ask_is("LOG OFF NOW? Y/N");
    CHECK(blank_between(ROW_ASK, 17, NL_COLS));
    leaving = 0; display_prompt();
    ask_is("START WITH HOW MANY SEATS? 2-6");
}
static void test_a_new_prompt_leaves_nothing_of_the_old_one(void)
{
    pr_kind = 'D'; pr[1] = 1; pr[2] = 0; pr[3] = 7; edit[0] = '3'; edit_len = 1; display_prompt();
    pr_kind = 'S'; edit_len = 0; display_prompt();
    CHECK(blank_between(ROW_INFO, 0, NL_COLS));
    CHECK(blank_between(ROW_ASK, 31, NL_COLS));
    strip_is_purple();
}
static void test_the_clock_counts_down_in_the_strip(void)
{
    display_clock(31);
    CHECK(at_text(ROW_ASK, 76, "   ") && blank_between(ROW_ASK, 0, NL_COLS));       /* no prompt: no clock */
    pr_kind = 'S'; display_prompt(); display_clock(31);
    CHECK(at_text(ROW_ASK, 76, " 30"));
    display_clock(10);
    CHECK(at_text(ROW_ASK, 76, "  9"));
    display_clock(0);
    CHECK(at_text(ROW_ASK, 76, "   "));
    CHECK(CELL(ROW_ASK, 77).bg == C_STRIP);
}
static void test_game_over_names_the_winners(void)
{
    seat(0, SEAT_HUMAN, "ANDY-9", 0); seat(1, SEAT_BOT, "WIREWOLF", 0);
    winners = 1; display_game_over();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "GAME OVER. TOP NETRUNNER: ANDY-9"));
    winners = 3; display_game_over();
    CHECK(at_text(ROW_INFO, COL_PROMPT, "TOP NETRUNNERS: ANDY-9 WIREWOLF"));
    CHECK(blank_between(ROW_INFO, COL_PROMPT + 31, NL_COLS));
}
static void test_a_message_takes_the_question_line(void)
{
    display_message("NO CARRIER. REDIALING...");
    ask_is("NO CARRIER. REDIALING...");
    CHECK(CELL(ROW_ASK, 5).bg == C_STRIP && blank_between(ROW_ASK, 25, NL_COLS));
}
static void test_log_view_steps_back(void)
{
    display_playfield();
    push_log("[$] ONE"); push_log("[!] TWO"); push_log("THREE"); push_log("FOUR");
    log_back = 0; display_log_view();
    CHECK(at_text(ROW_LOG, COL_LOGTEXT, "TWO") && at_text(ROW_LOG + 1, COL_LOGTEXT, "THREE") && at_text(ROW_LOG + 2, COL_LOGTEXT, "FOUR"));
    log_back = 1; display_log_view();
    CHECK(at_text(ROW_LOG, COL_LOGTEXT, "ONE") && at_text(ROW_LOG + 1, COL_LOGTEXT, "TWO") && at_text(ROW_LOG + 2, COL_LOGTEXT, "THREE"));
    CHECK(CELL(ROW_LOG, COL_LOGMARK + 1).fg == C_GREEN);
    log_back = 3; display_log_view();
    CHECK(blank_between(ROW_LOG, 0, NL_COLS) && blank_between(ROW_LOG + 1, 0, NL_COLS) && at_text(ROW_LOG + 2, COL_LOGTEXT, "ONE"));
}

/* ---- the title and the instruction pages */
static void set_region(const char *r)
{
    region_len = (unsigned char)strlen(r);
    memcpy(region, r, region_len);
}
static void test_title_shows_region_status_and_handle(void)
{
    set_region("TOKYO"); status[0] = 0; edit_len = 0; refused = 0;
    display_title_screen();
    CHECK(at_text(ROW_REGION, COL_LABEL, "REGION") && at_text(ROW_REGION, COL_VALUE, "TOKYO"));
    CHECK(at_text(ROW_REGION + 1, COL_LABEL, "STATUS") && at_text(ROW_REGION + 2, COL_LABEL, "HANDLE"));
    CHECK(CELL(ROW_REGION, COL_LABEL).fg == C_CYAN && CELL(ROW_REGION, COL_VALUE).fg == C_WHITE);
    CHECK(at_text(ROW_ASK, COL_PROMPT, "TYPE A HANDLE, THEN ENTER. ESC QUITS"));
}
static void test_title_has_the_logo_above_the_lines(void)
{
    unsigned char r, c;
    unsigned int n = 0;
    display_title_screen();
    for (r = 1; r < ROW_REGION; ++r)
        for (c = 0; c < NL_COLS; ++c)
            if (nl_cells[r][c].ch >= 0x2500 && nl_cells[r][c].ch < 0x2600)
                ++n;
    CHECK(n > 150);
    CHECK(blank_between(ROW_BAR, 0, NL_COLS) || nl_cells[ROW_BAR][0].bg == C_BG);
}
static void test_title_status_no_carrier_and_server_text(void)
{
    status[0] = 0; status[1] = NO_CARRIER; display_title_screen();
    CHECK(at_text(ROW_REGION + 1, COL_VALUE, "NO CARRIER") && CELL(ROW_REGION + 1, COL_VALUE).fg == C_RED);
    status[0] = 8; status[1] = 0; memcpy(status + 2, "3 OPEN!", 7);
    display_title_update();
    CHECK(at_text(ROW_REGION + 1, COL_VALUE, "3 OPEN!") && blank_between(ROW_REGION + 1, COL_VALUE + 7, NL_COLS));
}
static void test_title_handle_shows_the_typed_text_and_a_cursor(void)
{
    status[0] = 0; edit_len = 3; memcpy(edit, "ACE", 3);
    display_title_screen();
    CHECK(at_text(ROW_REGION + 2, COL_VALUE, "ACE") && CELL(ROW_REGION + 2, COL_VALUE + 3).bg != C_BG);
    edit_len = 0; display_title_update();
    CHECK(blank_between(ROW_REGION + 2, COL_VALUE + 1, COL_VALUE + 8) && CELL(ROW_REGION + 2, COL_VALUE).bg != C_BG);
}
static void test_title_update_touches_nothing_above_the_region(void)
{
    nl_cell snap[ROW_REGION][NL_COLS];
    edit_len = 0; display_title_screen();
    memcpy(snap, nl_cells, sizeof snap);
    edit_len = 2; memcpy(edit, "XY", 2); status[0] = 0; status[1] = NO_CARRIER;
    display_title_update();
    CHECK(memcmp(snap, nl_cells, sizeof snap) == 0);
    CHECK(at_text(ROW_REGION + 2, COL_VALUE, "XY"));
}
static void test_title_refusals_replace_the_hint_line(void)
{
    refused = REFUSED_LATE; display_title_screen();
    CHECK(at_text(ROW_ASK, COL_PROMPT, "TOO LATE: THE LAST SEAT WAS TAKEN"));
    refused = REFUSED_HANDLE; display_title_screen();
    CHECK(at_text(ROW_ASK, COL_PROMPT, "THAT HANDLE IS TAKEN"));
    refused = 0;
}
static void test_title_key_line_names_the_keys(void)
{
    display_title_screen();
    CHECK(at_text(ROW_KEYS, COL_LABEL, "TAB") && at_text(ROW_KEYS + 1, COL_LABEL, "^S") && at_text(ROW_KEYS + 2, COL_LABEL, "?"));
    CHECK(CELL(ROW_KEYS, COL_LABEL).fg == C_CYAN && at_text(ROW_KEYS, COL_KEYS, "MOVE HINTS"));
    hints = 1; display_title_update();
    CHECK(at_text(ROW_KEYS, COL_HINTS, "ON ")); hints = 0; display_title_update();
    CHECK(at_text(ROW_KEYS, COL_HINTS, "OFF"));
}
static void test_help_pages_open_over_the_screen_and_restore_it(void)
{
    static nl_cell before[NL_ROWS][NL_COLS];
    unsigned char r, c, k, n = 0;
    display_playfield();
    memcpy(before, nl_cells, sizeof before);
    help_open();
    CHECK(memcmp(before, nl_cells, sizeof before) != 0);
    for (r = 0; r < NL_ROWS; ++r)
        for (c = 0; c < NL_COLS; ++c)
            if (nl_cells[r][c].ch == 'H') ++n;
    CHECK(at_text(HELP_ROW + 1, HELP_COL + 2, "HOW TO PLAY"));
    CHECK(help_key('<') == 1);                          /* the first page stays */
    CHECK(at_text(HELP_ROW + 1, HELP_COL + 2, "HOW TO PLAY"));
    CHECK(help_key('>') == 1 && at_text(HELP_ROW + 1, HELP_COL + 2, "YOUR TURN"));
    for (k = 0; k < 5; ++k) help_key('>');             /* the last page stays */
    CHECK(at_text(HELP_ROW + 1, HELP_COL + 2, "KEYS"));
    CHECK(help_key('<') == 1 && at_text(HELP_ROW + 1, HELP_COL + 2, "YOUR TURN"));
    CHECK(help_key(K_ESC) == 0);
    CHECK(memcmp(before, nl_cells, sizeof before) == 0);
}
static void test_help_closes_on_return_and_the_help_key_too(void)
{
    static nl_cell before[NL_ROWS][NL_COLS];
    display_title_screen(); memcpy(before, nl_cells, sizeof before);
    help_open(); CHECK(help_key(K_RETURN) == 0 && memcmp(before, nl_cells, sizeof before) == 0);
    help_open(); CHECK(help_key(K_HELP) == 0 && memcmp(before, nl_cells, sizeof before) == 0);
    help_open(); CHECK(help_key('x') == 1); CHECK(help_key(K_ESC) == 0);
}
static void test_help_panel_is_framed_inside_the_screen_and_counts_pages(void)
{
    display_playfield(); help_open();
    CHECK(CELL(HELP_ROW, HELP_COL).ch == 0x250C && CELL(HELP_ROW, HELP_COL + HELP_W - 1).ch == 0x2510);
    CHECK(CELL(HELP_ROW + HELP_H - 1, HELP_COL).ch == 0x2514 && CELL(HELP_ROW + HELP_H - 1, HELP_COL + HELP_W - 1).ch == 0x2518);
    CHECK(HELP_ROW + HELP_H <= ROW_INFO && HELP_COL + HELP_W <= NL_COLS);
    CHECK(at_text(HELP_ROW + 1, HELP_COL + HELP_W - 6, "1/3"));
    help_key(K_ESC);
}
static void test_a_huge_hive_keeps_its_bar_inside_the_column(void)
{
    hive_size[3] = 108;
    display_playfield();
    CHECK(CELL(ROW_GRID + 3, COL_BAR + BAR_W - 1).bg == C_SYN0 + 3);
    CHECK(CELL(ROW_GRID + 3, COL_BAR + BAR_W).bg != C_SYN0 + 3 && CELL(ROW_GRID + 3, COL_NAME).fg == C_SYN0 + 3);
    CHECK(at_text(ROW_GRID + 3, COL_NAME, "IRON VECTOR"));
}
static void test_game_updates_while_help_is_up_wait_behind_it(void)
{
    static nl_cell panel[NL_ROWS][NL_COLS];
    display_playfield();
    help_open();
    memcpy(panel, nl_cells, sizeof panel);
    msg[0] = 'l'; msg[1] = 12; memcpy(msg + 2, "[$] MID HELP", 12);
    display_log();
    display_seat(1);
    CHECK(memcmp(panel, nl_cells, sizeof panel) == 0);          /* the panel stays as drawn */
    help_key(K_ESC);
    CHECK(at_text(ROW_LOG + LOG_ROWS - 1, COL_LOGTEXT, "MID HELP"));   /* and the screen has caught up */
}

int main(void)
{
    RUN(test_title_bar); RUN(test_region_and_link_from_the_server); RUN(test_grid_headers_and_cells);
    RUN(test_hint_digits_only_on_empty_runnable_nodes); RUN(test_syndicate_panel_sizes_bars_and_ice);
    RUN(test_a_hive_of_one_still_shows_a_bar); RUN(test_safe_and_end_line); RUN(test_last_move); RUN(test_seats);
    RUN(test_display_seat_changes_only_its_row); RUN(test_display_node_changes_only_its_cells);
    RUN(test_the_last_move_flashes_then_settles); RUN(test_log_scrolls_and_marks);
    RUN(test_waiting_for_another_seat_shows_the_kit); RUN(test_in_the_lobby_the_strip_is_empty); RUN(test_start_prompt);
    RUN(test_exploit_prompt); RUN(test_broker_prompt_names_the_syndicates_by_colour); RUN(test_tie_prompt_both_questions);
    RUN(test_hivejack_prompt_cash_out_then_swap); RUN(test_buy_prompt_and_the_buy_panel);
    RUN(test_buy_step_two_with_nothing_bought_shows_only_the_question); RUN(test_a_complaint_replaces_the_question);
    RUN(test_logging_off_asks_then_the_prompt_comes_back); RUN(test_a_new_prompt_leaves_nothing_of_the_old_one);
    RUN(test_the_clock_counts_down_in_the_strip); RUN(test_game_over_names_the_winners);
    RUN(test_a_message_takes_the_question_line); RUN(test_log_view_steps_back);
    RUN(test_title_shows_region_status_and_handle); RUN(test_title_has_the_logo_above_the_lines);
    RUN(test_title_status_no_carrier_and_server_text); RUN(test_title_handle_shows_the_typed_text_and_a_cursor);
    RUN(test_title_update_touches_nothing_above_the_region); RUN(test_title_refusals_replace_the_hint_line);
    RUN(test_title_key_line_names_the_keys); RUN(test_help_pages_open_over_the_screen_and_restore_it);
    RUN(test_help_closes_on_return_and_the_help_key_too); RUN(test_help_panel_is_framed_inside_the_screen_and_counts_pages);
    RUN(test_a_huge_hive_keeps_its_bar_inside_the_column); RUN(test_game_updates_while_help_is_up_wait_behind_it);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
