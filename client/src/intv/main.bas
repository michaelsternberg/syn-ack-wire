' ===========================================================================
' main.bas -- SYN-ACK WIRE for the Intellivision and the FujiNet cartridge:
' the title, connecting, then frames in and keys out, forever. As
' src/common/main.c, in IntyBASIC: the title dials at once and asks for the
' table's status (I), which the server keeps up to date there; ENTER joins
' on the same connection. A full table sends the player back to the title,
' still connected. A dropped link reconnects with the seat's token, and the
' server resends everything. Options, then 3, then 1 logs off for good:
' back to the title, a bot in the seat. The server sends a keepalive every
' 10 s, so 20 s of silence shows NO SIGNAL, and 30 s redials.
'
' Built by the client's Makefile (make intv): tools/bas_pp.py fills in the
' {NAME}s and the '#if lines, IntyBASIC 1.5 compiles, as1600 assembles.
' The code is GPL-3.0 (LICENSE).
' ===========================================================================

    OPTION MAP 2
    OPTION EXPLICIT ON
    ASM CFGVAR "name" = "SYN-ACK WIRE"
    ASM CFGVAR "short_name" = "SYN-ACK"
    ASM CFGVAR "year" = 2026

    GOTO boot

    INCLUDE "memory.bas"
    INCLUDE "fujinet.bas"
    INCLUDE "net.bas"
    INCLUDE "proto.bas"
    INCLUDE "ecskbd.bas"
    INCLUDE "input.bas"
    INCLUDE "sound.bas"
    INCLUDE "isr.bas"

    ' $A000-$B7FF ($B800 up shows GRAM on some consoles)
    SEGMENT 1
    INCLUDE "display.bas"
    SEGMENT 0
    INCLUDE "title.bas"
    ' $4810-$4FFF
    SEGMENT 5
    INCLUDE "help_pages.bas"
    ' $C040-$DFFF ($E000-$EFFF is the ECS's ROM: tools/checkcfg.py)
    SEGMENT 2
    INCLUDE "prompt.bas"
    INCLUDE "game.bas"
    INCLUDE "help.bas"
'#if AUTOPLAY
    INCLUDE "autoplay.bas"
'#endif

    SEGMENT 0

    CONST NO_SIGNAL = 20        ' seconds without a frame: say so
    CONST REDIAL    = 30        ' and hang up and dial again

' ---------------------------------------------------------------------------
boot:
    POKE TEST_HOOK, 0           ' (cartridge RAM: a reset leaves it as it was)
    FOR i = 0 TO 199            ' and the state a new game starts from
        POKE BOARD + i, 0       ' (BOARD .. COINS, SKIND, SNAME)
    NEXT i
    FOR i = 0 TO 59
        POKE LOGROW + i, 32
    NEXT i
    FOR i = 0 TO RULE_KIT - 1
        POKE KIT + i, NONE
    NEXT i
    FOR i = 0 TO 3
        POKE TOKEN + i, 0
    NEXT i
    POKE STATUS, 0 : POKE STATUS + 1, 0
    turn = NONE : last_node = NONE : blink = NONE
    hints = {HINTS}
    snd_on = 1
    tps = 60 : IF NTSC = 0 THEN tps = 50
    DEFINE 0, 2, yen_card
    WAIT
    ON FRAME GOSUB frame_isr
    GOSUB ecs_init
    scr_mode = 1                ' the title's start switches to colour stack
    GOSUB display_title_screen
    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN
        dmsg = M_NOFUJI : GOSUB display_message
        snow_on = 0
nofuji: GOTO nofuji
    END IF
'#if LOBBY_KEY
    ' The player's lobby name starts the handle: upper case, and only what
    ' the title lets a player type.
    ak_key = 0 : GOSUB net_appkey
    i = 0
    WHILE i < fn_len AND edit_len < 8
        c = rb(AKBUF + 2 + i)
        IF c >= 97 AND c <= 122 THEN c = c - 32
        IF c > 32 AND c <= 90 THEN POKE EDIT + edit_len, c : edit_len = edit_len + 1
        i = i + 1
    WEND
'#endif
    dial = edit_len
    IF dial = 8 THEN dial = 7

' ---------------------------------------------------------------------------
' The title: type a handle, ENTER to jack in. While the player types, the
' status shows what joining would do (or NO CARRIER: ENTER dials again).
title:
    IF link_up = 0 THEN POKE STATUS, 0 : POKE STATUS + 1, 0
    leaving = 0 : opting = 0
    GOSUB display_title_screen
    IF link_up = 0 THEN         ' dial while it shows
        GOSUB dial
        IF net_err THEN
            GOSUB net_close
            POKE STATUS + 1, NO_CARRIER
        ELSE
            link_up = 1
            POKE TX0, 73 : POKE TX0 + 1, PROTO_VERSION : POKE TX0 + 2, {TABLE}
            n = 3 : GOSUB net_send
        END IF
        GOSUB display_title_update
    END IF
title_loop:
    WAIT
    IF link_up THEN
        GOSUB net_poll
        IF net_err THEN
            link_up = 0
            POKE STATUS, 0 : POKE STATUS + 1, NO_CARRIER
            GOSUB net_close
            GOSUB display_title_update
        END IF
        GOSUB proto_next
        WHILE pk                ' only "a" and "i" come before joining
            GOSUB game_frame
            GOSUB display_title_update
            GOSUB proto_next
        WEND
    END IF
    GOSUB display_title_idle
    GOSUB key
    IF c = 0 THEN GOTO title_loop
    IF opting THEN              ' 1 hints, 2 sound; any other key closes them
        IF c = 49 THEN
            hints = hints XOR 1
        ELSEIF c = 50 THEN
            GOSUB snd_toggle
        ELSE
            opting = 0
        END IF
        GOSUB display_title_update
        GOTO title_loop
    END IF
    IF c = K_RETURN AND edit_len > 0 THEN
        refused = 0             ' REFUSED_LATE, should it turn out full
        IF rb(STATUS + 1) <> ST_FULL THEN refused = REFUSED_LATE
        GOTO play
    END IF
    IF c = K_ESC THEN opting = 1 : GOSUB display_title_update : GOTO title_loop
    GOSUB title_key
    GOTO title_loop

' ---------------------------------------------------------------------------
' The game, from joining on.
play:
    FOR i = 0 TO 7
        IF i < edit_len THEN POKE HANDLE + i, rb(EDIT + i) ELSE POKE HANDLE + i, 32
    NEXT i
    handle_len = edit_len
    GOSUB connect
play_loop:
    WAIT
    GOSUB net_poll
    IF net_err THEN dmsg = M_LOST : GOSUB redial : GOTO play_loop
    GOSUB proto_next
    WHILE pk
        quiet = 0
        #q0 = FRAME
        IF link_up = 0 THEN link_up = 1 : GOSUB display_link
        IF pk = PROTO_BAD THEN          ' lost our place in the stream
            POKE TX0, 82 : n = 1 : GOSUB net_send
            pk = 0
        ELSEIF pk = 102 THEN            ' f: a fault
            e = rb(MSG + 1)
            IF e = F_TOKEN THEN         ' the server forgot us: join as new
                FOR i = 0 TO 3
                    POKE TOKEN + i, 0
                NEXT i
                link_up = 0
                GOSUB net_close
                GOSUB connect
                GOTO play_loop
            END IF
            IF e = F_HANDLE THEN refused = REFUSED_HANDLE
            IF e = F_FULL OR e = F_HANDLE THEN GOTO title      ' still connected, watching again
            IF e = F_VERSION THEN
                dmsg = M_VERSION : GOSUB display_message
stuck:          GOTO stuck
            END IF
            GOSUB proto_next
        ELSE
            GOSUB game_frame
            GOSUB proto_next
        END IF
    WEND
    IF FRAME - #q0 >= tps THEN
        #q0 = #q0 + tps
        quiet = quiet + 1
        IF quiet = NO_SIGNAL THEN link_up = 0 : GOSUB display_link
        IF quiet = REDIAL THEN dmsg = M_NOSIG : GOSUB redial : GOTO play_loop
    END IF
    GOSUB display_idle
    GOSUB key                   ' always read, so no stale key answers the next prompt
    IF c = 0 THEN GOTO play_clock
    IF opting THEN              ' 1 hints, 2 sound, 3 log off; any other key closes them
        IF c = 49 THEN
            GOSUB toggle_hints
        ELSEIF c = 50 THEN
            GOSUB snd_toggle
        ELSEIF c = 51 THEN
            opting = 0 : leaving = 1
        ELSE
            opting = 0
        END IF
        GOSUB display_prompt
        GOTO play_clock
    END IF
    IF leaving THEN             ' log off? 1 (Y) leaves, any other key stays
        IF c = 49 OR c = 89 THEN GOSUB log_off : GOTO title
        leaving = 0
        GOSUB display_prompt
        GOTO play_clock
    END IF
    IF c = K_ESC THEN opting = 1 : GOSUB display_prompt : GOTO play_clock
    IF c = K_OPTION THEN GOSUB toggle_hints : GOTO play_clock
    IF c = K_NEXT OR c = K_RIGHT OR c = K_DOWN THEN dk = 1 : GOSUB next_view : GOTO play_clock
    IF c = K_LEFT OR c = K_UP THEN dk = 2 : GOSUB next_view : GOTO play_clock
    IF pr_kind = 0 THEN GOTO play_clock
    ' The keypad's digits: a syndicate by its panel row; 1 and 0 yes and no
    IF pr_kind = P_BUY AND step = 1 THEN
        IF c = 49 THEN c = 89
        IF c = 48 THEN c = 78
    ELSEIF pr_kind = P_FOUND OR pr_kind = P_TIE OR pr_kind = P_BUY THEN
        IF c >= 49 AND c < 49 + NSYN THEN c = syn_letter(c - 49) + 32
    END IF
    gk = c
    GOSUB game_key
play_clock:
    IF pr_kind <> 0 AND secs <> 0 THEN
        IF FRAME - #t0 >= tps THEN
            #t0 = #t0 + tps
            secs = secs - 1
            GOSUB clock
            IF secs = 0 THEN GOSUB clear_prompt     ' time's up: the seat's bot answers
        END IF
    END IF
    GOTO play_loop

' ---------------------------------------------------------------------------
' key: the next key in c, or 0. The top buttons open the instructions, on
' the title or mid-game; while they're up the keys are theirs, and the loop
' goes on reading the network.
key: PROCEDURE
'#if AUTOPLAY
    GOSUB auto_key
'#else
    GOSUB in_key
'#endif
    c = in_k
    IF helping THEN
        IF c THEN
            GOSUB help_key
            IF helping = 0 THEN
                IF field THEN GOSUB draw_view : GOSUB display_prompt ELSE GOSUB display_title_screen
            END IF
        END IF
        c = 0
        RETURN
    END IF
    IF c = K_HELP THEN GOSUB help_open : c = 0
END

' dial: the server the lobby picked (AppKey LOBBY_KEY: "TCP://host:port/"),
' else the one built in. net_err = 0 if it answered.
dial: PROCEDURE
'#if LOBBY_KEY
    ak_key = {LOBBY_KEY} : GOSUB net_appkey
    IF fn_len THEN
        POKE FN_TX, 78 : POKE FN_TX + 1, 58     ' "N:"
        CALL COPYB(AKBUF + 2, FN_TX + 2, fn_len)
        #fn_txlen = fn_len + 2
        GOSUB net_open
        RETURN
    END IF
'#endif
    FOR i = 0 TO {#URL} - 1
        POKE FN_TX + i, url_text(i) + 32
    NEXT i
    #fn_txlen = {#URL}
    GOSUB net_open
END

hello: PROCEDURE
    POKE TX0, 72 : POKE TX0 + 1, PROTO_VERSION : POKE TX0 + 2, {TABLE}
    CALL COPYB(TOKEN, TX0 + 3, 4)
    CALL COPYB(HANDLE, TX0 + 7, 8)
    n = 15 : GOSUB net_send
END

' connect: join, on the title's connection if it has one, else dial until
' it answers.
connect: PROCEDURE
    IF link_up = 0 THEN
        dmsg = M_DIALING : GOSUB display_message
        GOSUB dial
        WHILE net_err
            dmsg = M_NOCARR : GOSUB display_message
            GOSUB net_close
            GOSUB pause
            GOSUB dial
        WEND
    END IF
    GOSUB hello
    quiet = 0
    #q0 = FRAME
END

' redial: the link is down (dmsg says why): hang up, wait, dial again.
redial: PROCEDURE
    link_up = 0
    GOSUB display_link
    GOSUB display_message
    GOSUB net_close
    GOSUB pause
    GOSUB connect
END

pause: PROCEDURE                ' 2 seconds
    FOR e = 1 TO 2 * tps
        WAIT
    NEXT e
END

' log_off: leave the game for good. The server hands the seat to a bot and
' forgets the token, so this client goes back to the title as a newcomer.
log_off: PROCEDURE
    POKE TX0, 81 : n = 1 : GOSUB net_send
    WHILE txq_len OR nx_st      ' out before the close
        WAIT
        GOSUB net_poll
        IF net_err THEN EXIT WHILE
    WEND
    GOSUB net_close
    FOR i = 0 TO 3
        POKE TOKEN + i, 0
    NEXT i
    link_up = 0 : leaving = 0 : refused = 0 : opting = 0
    CALL COPYB(HANDLE, EDIT, 8)     ' the prompts used it: the handle again
    edit_len = handle_len
    dial = edit_len
    IF dial = 8 THEN dial = 7
END

url_text:
    DATA "{URL}"

' The yen sign (GRAM card 0)
yen_card:
    BITMAP "X.....X."
    BITMAP ".X...X.."
    BITMAP "..X.X..."
    BITMAP "XXXXXXX."
    BITMAP "...X...."
    BITMAP "XXXXXXX."
    BITMAP "...X...."
    BITMAP "........"
' and an underline (GRAM card 1)
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP "XXXXXXX."
    BITMAP "........"
