/* Host tests for src/amiga/net_fn.c: the network calls of platform.h over a scripted fake of fujinet-nio-lib.
 *   make test-amiga-net */
#include <stdio.h>
#include <string.h>
#include "fujinet-nio.h"
#include "../src/platform.h"

/* ---- the fake library: a script of reads, and a record of what was asked */
struct step { uint8_t rc; const char *bytes; uint16_t n; uint8_t flags; };
static struct step script[16];
static int nscript, cur;
static uint16_t cur_used;
static int opens, reads, writes, closes;
static char open_host[80];
static uint16_t open_port;
static uint32_t last_read_off, last_write_off;
static uint16_t biggest_read_request;
static uint8_t open_rc;
static int busy_depth, busy_on, busy_off, busy_during_open, busy_during_close, busy_bad;

static void script_clear(void) { memset(script, 0, sizeof script); nscript = cur = 0; cur_used = 0; }
static void script_add(uint8_t rc, const char *bytes, uint16_t n, uint8_t flags)
{
    script[nscript].rc = rc; script[nscript].bytes = bytes; script[nscript].n = n; script[nscript].flags = flags;
    nscript++;
}

uint8_t fn_init(void) { return FN_OK; }
static int in_open_seen_busy, in_close_seen_busy;
uint8_t fn_tcp_open(fn_handle_t *h, const char *host, uint16_t port)
{
    in_open_seen_busy = busy_depth > 0;
    ++opens; strncpy(open_host, host, sizeof open_host - 1); open_port = port; *h = 7;
    return open_rc;
}
uint8_t fn_write(fn_handle_t h, uint32_t off, const uint8_t *d, uint16_t len, uint16_t *w)
{
    (void)h; (void)d; ++writes; last_write_off = off; *w = len; return FN_OK;
}
uint8_t fn_read(fn_handle_t h, uint32_t off, uint8_t *buf, uint16_t max, uint16_t *got, uint8_t *flags)
{
    struct step *s;
    uint16_t n;
    (void)h;
    ++reads; last_read_off = off;
    if (max > biggest_read_request) biggest_read_request = max;
    *got = 0; *flags = 0;
    if (cur >= nscript) return FN_ERR_NOT_READY;
    s = &script[cur];
    if (s->rc) { ++cur; return s->rc; }
    n = s->n - cur_used;
    if (n > max) n = max;
    memcpy(buf, s->bytes + cur_used, n);
    cur_used += n; *got = n;
    if (cur_used == s->n) { *flags = s->flags; ++cur; cur_used = 0; }
    return FN_OK;
}
uint8_t fn_close(fn_handle_t h) { (void)h; in_close_seen_busy = busy_depth > 0; ++closes; return FN_OK; }

/* ---- the platform's clock, as the test drives it */
static unsigned char now;
unsigned char ticks(void) { return now; }
unsigned char ticks_per_sec(void) { return 50; }

/* ---- the harness */
static int saves;
void pointer_busy(unsigned char on)       /* pointer.c's, counted here */
{
    if (on) { ++busy_on; ++busy_depth; } else { ++busy_off; if (--busy_depth < 0) ++busy_bad; }
}
void amiga_prefs_save(void) { ++saves; }    /* prefs_io.c's, counted here */

static int failures, ran;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
#define RUN(t) do { ++ran; printf("%s\n", #t); reset(); t(); } while (0)

static void reset(void)
{
    saves = 0;
    busy_depth = busy_on = busy_off = busy_during_open = busy_during_close = busy_bad = 0;
    script_clear();
    opens = reads = writes = closes = 0; open_host[0] = 0; open_port = 0; open_rc = FN_OK;
    last_read_off = last_write_off = 0; biggest_read_request = 0; now = 0;
    rx_head = rx_tail = 0;
}
static void open_ok(void) { CHECK(net_open("N:TCP://127.0.0.1:6599/") == 0); }

static void test_open_shows_the_busy_pointer_and_puts_it_back(void)
{
    open_ok();
    CHECK(busy_on == 1 && busy_off == 1 && busy_depth == 0);
    CHECK(in_open_seen_busy);           /* the pointer was busy while the call was made */
}
static void test_a_failed_open_puts_it_back_too(void)
{
    open_rc = FN_ERR_NOT_READY;
    CHECK(net_open("N:TCP://127.0.0.1:6599/") != 0);
    CHECK(busy_on == 1 && busy_off == 1 && busy_depth == 0);
}
static void test_a_bad_url_never_raises_it(void)
{
    CHECK(net_open("N:TCP://nowhere/") != 0);
    CHECK(busy_on == busy_off && busy_depth == 0);
}
static void test_close_is_busy_only_when_open(void)
{
    open_ok();
    busy_on = busy_off = 0;
    net_close();
    CHECK(busy_on == 1 && busy_off == 1 && busy_depth == 0 && in_close_seen_busy);
    net_close();                        /* already closed: nothing to wait for */
    CHECK(busy_on == 1 && busy_off == 1);
}
static void test_polling_never_flickers_the_pointer(void)
{
    open_ok();
    busy_on = busy_off = 0;
    script_add(FN_OK, "ab", 2, 0);
    now = 10; net_poll(); now = 20; net_poll(); net_send(1);
    CHECK(busy_on == 0 && busy_off == 0);
}

static void test_url_parsed(void)
{
    CHECK(net_open("N:TCP://syn-ack-wire.fly.dev:6502/") == 0);
    CHECK(strcmp(open_host, "syn-ack-wire.fly.dev") == 0);
    CHECK(open_port == 6502);
}
static void test_url_without_port_rejected(void)
{
    CHECK(net_open("N:TCP://syn-ack-wire.fly.dev/") != 0);
    CHECK(net_open("N:TCP://host:99999/") != 0);
    CHECK(opens == 0);
}
static void test_open_failure_passed_on(void)
{
    open_rc = FN_ERR_TRANSPORT;
    CHECK(net_open("N:TCP://127.0.0.1:6599/") == FN_ERR_TRANSPORT);
}
static void test_split_frame_kept(void)
{
    open_ok();
    script_add(FN_OK, "abc", 3, 0);
    script_add(FN_OK, "0123456789012345678901234567890123456789", 40, 0);
    CHECK(net_poll() == 0); CHECK(net_poll() == 0);
    CHECK(rx_tail == 43);
    CHECK(memcmp(rx_ring, "abc0123456789", 13) == 0);
}
static void test_not_ready_is_quiet(void)
{
    open_ok();
    CHECK(net_poll() == 0);
    CHECK(reads == 1 && rx_tail == 0);
}
static void test_quiet_paces_the_asking(void)
{
    open_ok();
    CHECK(net_poll() == 0); CHECK(reads == 1);
    CHECK(net_poll() == 0); CHECK(reads == 1);          /* same tick: no new call */
    now += 5;                                           /* a tenth of a second at 50 Hz */
    CHECK(net_poll() == 0); CHECK(reads == 2);
}
static void test_data_polls_at_once(void)
{
    open_ok();
    script_add(FN_OK, "xy", 2, 0);
    CHECK(net_poll() == 0); CHECK(reads == 1);
    CHECK(net_poll() == 0); CHECK(reads == 2);          /* bytes came: ask again in the same tick */
}
static void test_send_wakes_the_poll(void)
{
    open_ok();
    CHECK(net_poll() == 0); CHECK(reads == 1);
    tx[0] = 'R';
    CHECK(net_send(1) == 0);
    CHECK(net_poll() == 0); CHECK(reads == 2);
}
static void test_ring_never_overruns(void)
{
    char big[600];
    int i, before;
    memset(big, 'z', sizeof big);
    open_ok();
    script_add(FN_OK, big, sizeof big, 0);
    for (i = 0; i < 20; ++i) CHECK(net_poll() == 0);    /* the consumer (rx_head) takes nothing */
    CHECK((unsigned char)(rx_tail - rx_head) <= 255);   /* tail never laps head: full means 255 */
    before = (unsigned char)(rx_tail - rx_head);
    CHECK(before == 255);
    rx_head = 100;                                      /* the consumer takes 100 bytes: the rest flows in */
    CHECK(net_poll() == 0); CHECK(net_poll() == 0);
    CHECK((unsigned char)(rx_tail - rx_head) <= 255);
}
static void test_wrap(void)
{
    open_ok();
    rx_head = rx_tail = 250;
    script_add(FN_OK, "ABCDEFGHIJKLMNOPQRST", 20, 0);
    CHECK(net_poll() == 0); CHECK(net_poll() == 0);
    CHECK(rx_tail == (unsigned char)270);
    CHECK(memcmp(rx_ring + 250, "ABCDEF", 6) == 0);
    CHECK(memcmp(rx_ring, "GHIJKLMNOPQRST", 14) == 0);
}
static void test_offsets_sequential(void)
{
    open_ok();
    script_add(FN_OK, "abc", 3, 0);
    script_add(FN_OK, "defgh", 5, 0);
    net_poll(); CHECK(last_read_off == 0);
    net_poll(); CHECK(last_read_off == 3);
    net_poll(); CHECK(last_read_off == 8);
    tx[0] = 'I'; tx[1] = 3; tx[2] = 0;
    net_send(3); CHECK(last_write_off == 0);
    net_send(3); CHECK(last_write_off == 3);
}
static void test_blip_tolerated(void)
{
    int i;
    open_ok();
    for (i = 0; i < 4; ++i) script_add(FN_ERR_TRANSPORT, 0, 0, 0);
    for (i = 0; i < 4; ++i) { CHECK(net_poll() == 0); now += 5; }   /* 0.4 s of failures: not a dropped line */
    script_clear(); script_add(FN_OK, "k", 1, 0);
    CHECK(net_poll() == 0); CHECK(rx_tail == 1);
}
static void test_error_forgotten_after_a_good_exchange(void)
{
    int i;
    open_ok();
    script_add(FN_ERR_TRANSPORT, 0, 0, 0);
    CHECK(net_poll() == 0);                             /* one garbled reply, at tick 0 */
    script_clear();
    for (i = 0; i < 10; ++i) script_add(FN_ERR_NOT_READY, 0, 0, 0);
    for (i = 0; i < 10; ++i) { now += 20; CHECK(net_poll() == 0); }   /* four quiet seconds: the exchanges work */
    script_clear(); script_add(FN_ERR_TRANSPORT, 0, 0, 0);
    now += 20;
    CHECK(net_poll() == 0);                             /* a second blip is a blip, not a dead line */
}
static void test_dead_line_reported(void)
{
    int i, rc = 0;
    open_ok();
    for (i = 0; i < 12; ++i) script_add(FN_ERR_TRANSPORT, 0, 0, 0);
    for (i = 0; i < 12 && !rc; ++i) { rc = net_poll(); now += 20; }  /* 0.4 s apart: past 3 s soon */
    CHECK(rc == FN_ERR_TRANSPORT);
}
static void test_closed_flag(void)
{
    open_ok();
    script_add(FN_OK, "", 0, FN_READ_EOF);
    CHECK(net_poll() == NET_CLOSED);
}
static void test_eof_delivers_data_first(void)
{
    open_ok();
    script_add(FN_OK, "end", 3, FN_READ_EOF);
    CHECK(net_poll() == 0); CHECK(rx_tail == 3);        /* the bytes first */
    now += 5;
    CHECK(net_poll() == 0);                             /* not closed while the assembler has bytes to take */
    rx_head = rx_tail;
    now += 5;
    CHECK(net_poll() == NET_CLOSED);
}
static void test_read_cap(void)
{
    char big[300];
    memset(big, 'q', sizeof big);
    open_ok();
    script_add(FN_OK, big, sizeof big, 0);
    net_poll();
    CHECK(biggest_read_request <= 128);
}
static void test_hello_saves_the_handle(void)
{
    open_ok();
    CHECK(saves == 0);                                  /* dialling alone saves nothing: no handle is typed yet */
    tx[0] = 'H'; net_send(15);
    CHECK(saves == 1);
    tx[0] = 'P'; net_send(2);
    CHECK(saves == 1);                                  /* only the hello carries the handle */
}
static void test_close(void)
{
    open_ok();
    net_close();
    CHECK(closes == 1);
    CHECK(net_poll() == NET_CLOSED);
}

int main(void)
{
    RUN(test_open_shows_the_busy_pointer_and_puts_it_back); RUN(test_a_failed_open_puts_it_back_too);
    RUN(test_a_bad_url_never_raises_it); RUN(test_close_is_busy_only_when_open); RUN(test_polling_never_flickers_the_pointer);
    RUN(test_url_parsed); RUN(test_url_without_port_rejected); RUN(test_open_failure_passed_on);
    RUN(test_split_frame_kept); RUN(test_not_ready_is_quiet); RUN(test_quiet_paces_the_asking);
    RUN(test_data_polls_at_once); RUN(test_send_wakes_the_poll); RUN(test_ring_never_overruns); RUN(test_wrap);
    RUN(test_offsets_sequential); RUN(test_blip_tolerated); RUN(test_error_forgotten_after_a_good_exchange); RUN(test_dead_line_reported);
    RUN(test_closed_flag); RUN(test_eof_delivers_data_first); RUN(test_read_cap); RUN(test_hello_saves_the_handle); RUN(test_close);
    printf("%d tests, %d failures\n", ran, failures);
    return failures != 0;
}
