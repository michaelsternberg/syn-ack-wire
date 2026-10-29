' ===========================================================================
' memory.bas -- the cartridge RAM map, colours, key codes, and the variables
' and helpers every file shares. IntyBASIC is single-pass, so a CONST, DIM or
' DEF FN must come before its first use: this file is included first.
'
' IntyBASIC has about 220 8-bit and 50 16-bit variables, and 24 words of
' stack. Everything tabular lives in the FujiNet cartridge's RAM, $8000-$9BFF,
' 8 bits a location (fujinet.bas says why it stops there), reached with PEEK
' and POKE. {NAME} is filled in by tools/bas_pp.py from theme_tab.h,
' proto_tab.h and the Makefile before IntyBASIC sees the file.
' ===========================================================================

    ' ---- cartridge RAM ------------------------------------------------------
    CONST RING      = $8000     ' 1024: bytes from the server (net.bas)
    CONST RING_MASK = 1023
    CONST MSG       = $8400     ' 128: the frame being handled (proto.bas)
    CONST BOARD     = $8480     ' 108: a node a byte: 0 empty, 1 spike, 2.. hive of syndicate - 2
    CONST HSIZE     = $84F0     ' 7: each syndicate's hive size
    CONST CASHLO    = $8500     ' 6: each seat's cash in 100s, low bytes
    CONST CASHHI    = $8506     ' 6: and high bytes
    CONST COINS     = $8510     ' 42: seat * 7 + syndicate
    CONST SKIND     = $8540     ' 6: SEAT_EMPTY, _HUMAN, _AWAY, _BOT
    CONST SNAME     = $8548     ' 48: seat * 8, padded with spaces
    CONST KIT       = $8578     ' 6: node, bit 7 = can't run now; NONE = an empty slot
    CONST REGION    = $8580     ' 12
    CONST STATUS    = $8590     ' 30: length (0 none, else 1 + the text's), code, text
    CONST PR        = $85B0     ' 18: the open prompt: timeout, then its params
    CONST EDIT      = $85C8     ' 9: typed so far
    CONST TOKEN     = $85D4     ' 4: this seat's token
    CONST HANDLE    = $85D8     ' 8: as joined, padded with spaces
    CONST TXQ       = $85E0     ' 32: frames waiting to go out (net.bas)
    CONST LOGROW    = $8600     ' 60: the log's last 3 rows of 20, as SEATS shows them
    CONST LOGLINE   = $8640     ' 40: the newest log line, for the info row's ticker
    CONST AKBUF     = $8680     ' 72: an AppKey as read: its text from AKBUF + 2
    CONST KQ        = $8700     ' 16: the key ring: the controller's and the ECS's keys (input.bas)
    CONST KQ_MASK   = 15
    CONST TX0       = $8710     ' 16: the next frame to send, built in place (net_send)
    ' input.bas: TEST_HOOK, $8FF0-$8FF1; ecskbd.bas: ECS_HOOK, $8FF2

    ' ---- Intellivision colours ----------------------------------------------
    CONST C_BLACK    = 0
    CONST C_BLUE     = 1
    CONST C_RED      = 2
    CONST C_TAN      = 3
    CONST C_DKGREEN  = 4
    CONST C_GREEN    = 5
    CONST C_YELLOW   = 6
    CONST C_WHITE    = 7
    CONST C_GREY     = 8
    CONST C_CYAN     = 9
    CONST C_ORANGE   = 10
    CONST C_BROWN    = 11
    CONST C_PINK     = 12
    CONST C_LTBLUE   = 13
    CONST C_YELGREEN = 14
    CONST C_PURPLE   = 15

    ' A card's colours in foreground/background mode: foreground 0-7, any
    ' background (bit 13 is background bit 2, bit 12 bit 3: stic.txt).
    DEF FN fgbg(f, b) = ((f) + ((b) AND 3) * $200 + ((b) AND 4) * $800 + ((b) AND 8) * $200)
    ' A cartridge RAM byte (and a mailbox one: their upper bits aren't 0).
    DEF FN rb(a) = (PEEK(a) AND 255)

    ' The game screen's colours (foreground/background mode)
    CONST CW_TEXT  = fgbg(C_GREEN, C_BLACK)     ' labels, the log
    CONST CW_VALUE = fgbg(C_WHITE, C_BLACK)     ' numbers, handles
    CONST CW_MINE  = fgbg(C_YELLOW, C_BLACK)    ' yours: cash, crypto
    CONST CW_DIM   = fgbg(C_DKGREEN, C_BLACK)   ' empty nodes, nothing held
    CONST CW_INV   = fgbg(C_BLACK, C_GREEN)     ' the bar, whose turn it is, inverse text
    CONST CW_ASK   = fgbg(C_WHITE, C_PURPLE)    ' rows 10-11 while this seat decides
    CONST CW_ASKHI = fgbg(C_YELLOW, C_PURPLE)   ' the keys to press, what's typed
    CONST CW_ALERT = fgbg(C_WHITE, C_RED)       ' NO SIGNAL
    CONST CW_ICE   = fgbg(C_BLACK, C_WHITE)     ' a Black ICE hive's size
    CONST CW_RUN   = fgbg(C_YELLOW, C_BLACK)    ' a kit hint: that exploit can run here
    CONST CW_NORUN = fgbg(C_RED, C_BLACK)       ' ...and can't, yet
    CONST CW_BLANK = fgbg(C_BLACK, C_BLACK)
    CONST G_YEN    = $0800                      ' GRAM card 0: the yen sign
    CONST G_LINE   = $0808                      ' GRAM card 1: a line under a letter's place

    ' ---- screen -------------------------------------------------------------
    CONST ROW_INFO = 200                        ' row 10: what the prompt is about, or the log
    CONST ROW_ASK  = 220                        ' row 11: the question and the clock
    CONST COL_CLOCK = 18
    CONST V_GRID   = 0                          ' the views the disc steps through
    CONST V_SEATS  = 1
    CONST V_CRYPTO = 2

    ' ---- keys (in_key) ------------------------------------------------------
    CONST K_OPTION = 1                          ' move hints
    CONST K_HELP   = 2                          ' the instructions: the top buttons, ECS ?
    CONST K_NEXT   = 3                          ' the next view: the lower right button
    CONST K_DELETE = 8                          ' CLEAR, ECS left arrow
    CONST K_RETURN = 13                         ' ENTER, ECS RTN
    CONST K_ESC    = 27                         ' the options: the lower left button, ECS ESC
    CONST K_LEFT   = $1C                        ' the disc, the ECS arrows
    CONST K_RIGHT  = $1D
    CONST K_UP     = $1E
    CONST K_DOWN   = $1F

    ' ---- the game (theme_tab.h, proto_tab.h) --------------------------------
    CONST NSYN       = {NSYN}
    CONST NODES      = {RULE_ROWS} * {RULE_COLS}
    CONST SEATS      = {RULE_MAXPLAYERS}
    CONST RULE_COLS  = {RULE_COLS}
    CONST RULE_ROWS  = {RULE_ROWS}
    CONST RULE_KIT   = {RULE_KITSIZE}
    CONST RULE_MAXBUY = {RULE_MAXBUY}
    CONST RULE_SUPPLY = {RULE_COINSUPPLY}
    CONST RULE_SAFE  = {RULE_SAFESIZE}
    CONST RULE_MIN   = {RULE_MINPLAYERS}
    CONST GL_EMPTY   = {GLYPH_EMPTY}
    CONST GL_SPIKE   = {GLYPH_SPIKE}
    CONST NONE       = 255
    CONST CELL_SPIKE = 1
    CONST PROTO_VERSION = {PROTO_VERSION}
    CONST LEN_VAR    = {LEN_VAR}
    CONST LEN_BAD    = {LEN_BAD}
    CONST PROTO_BAD  = 63                       ' "?": the stream lost, send R
    CONST SEAT_EMPTY = {SEAT_EMPTY}
    CONST SEAT_HUMAN = {SEAT_HUMAN}
    CONST SEAT_AWAY  = {SEAT_AWAY}
    CONST SEAT_BOT   = {SEAT_BOT}
    CONST F_VERSION  = {F_VERSION}
    CONST F_FULL     = {F_FULL}
    CONST F_TOKEN    = {F_TOKEN}
    CONST F_HANDLE   = {F_HANDLE}
    CONST ST_FULL    = {ST_FULL}
    CONST NO_CARRIER = 255                      ' status code: the title couldn't dial
    CONST REFUSED_LATE = 1                      ' full, though the status showed a seat
    CONST REFUSED_HANDLE = 2                    ' that handle is taken there

    ' The prompts (pr_kind), as letters
    CONST P_SEATS  = 83                         ' S: the host's seat count
    CONST P_SPIKE  = 80                         ' P: run an exploit
    CONST P_FOUND  = 70                         ' F: broker a new hive
    CONST P_TIE    = 75                         ' K: a tie
    CONST P_DISP   = 68                         ' D: a hivejack's crypto
    CONST P_BUY    = 66                         ' B: buy

    ' Why the client refuses an answer itself (complaint), in display.bas
    CONST W_HOLD   = 1
    CONST W_EVEN   = 2
    CONST W_POOL   = 3
    CONST W_MAXBUY = 4
    CONST W_NOHIVE = 5
    CONST W_NONE   = 6
    CONST W_CASH   = 7
    CONST W_EMPTY  = 8
    CONST W_JOINS  = 9
    CONST W_NOSYN  = 10

    ' ---- state --------------------------------------------------------------
    ' The game, as the server last described it (game.bas writes it)
    DIM me, turn, last_node, winners, region_len, hints, link_up, refused
    DIM pr_kind, step, edit_len, complaint
    DIM leaving                 ' Esc, 3: the log-off question is up
    DIM opting                  ' Esc: the options are up
    DIM helping                 ' the instructions are up
    DIM view                    ' V_GRID, V_SEATS, V_CRYPTO
    DIM field                   ' the playfield is up (else the title)
    DIM handle_len, tps, secs, quiet, over_shown
    DIM #t0, #q0                ' FRAME when the clock and the silence last ticked
    DIM scr_mode                ' 0: colour stack (the title), 1: foreground/background
    DIM snow_on                 ' the frame handler rewrites the title's static
    DIM #snow, snow_part        ' its place in the random table, the part of the logo next (title.bas)

    ' The frame in MSG (proto.bas): its type, 0 = none
    DIM pk

    ' The key ring: the frame handler puts keys in (kq_push), in_key takes them
    DIM kq_head, kq_tail, kq_k, kq_n

    ' Main-loop scratch, shared by game.bas and display.bas (never the
    ' frame handler, which has its own)
    DIM i, j, n, v, b, c, e, k
    DIM #w, #v

    ' Drawing (display.bas): position, character, colour word
    DIM dp, dch, dn
    DIM #dc, #ds, #dv
