' ===========================================================================
' display.bas -- src/display.h's calls for the Intellivision: the game in
' 20 x 12 cards, foreground/background mode (GROM's upper case, a colour
' per card). The title is title.bas's.
'
' Three views, which the disc steps through (view); rows 10 and 11 are the
' same in each:
'   GRID     row 0 the column letters, rows 1-9 the Grid (the row digit, 12
'            nodes); beside it each syndicate: its keypad digit while a
'            prompt asks for a syndicate, its letter in its colours, its
'            hive's size and your crypto, or while buying its price; then
'            your cash and the last move
'   SEATS    row 0 the region (or NO SIGNAL), rows 1-6 the seats (marker,
'            handle, cash), rows 7-9 the log
'   CRYPTO   the seats by the syndicates: who holds what; then what is left
'            in the bank, the hives' sizes, and while buying their prices
'   row 10   what the open prompt is about, a refusal, or the newest log
'            line (scrolling when it is longer than 20)
'   row 11   the question and the seconds left
' Rows 10-11 turn purple while this seat has a decision to make. A call for
' a view that isn't up draws nothing: a view draws whole when it comes up.
'
' The game state is in cartridge RAM (memory.bas). Text arrives as ASCII
' $20-$5F; "\" from the server is money, shown as the yen sign (GRAM card 0).
' ===========================================================================

    CONST FLASH_FLIPS = 18      ' the last move blinks: 18 flips of 16 frames, about 5 s
    CONST FLASH_FRAMES = 16
    CONST TICK_FRAMES = 8       ' the ticker moves a character every 8 frames
    CONST TICK_PAUSE = 90       ' and rests this long at each end

    DIM dnode, dseat, dval, dr, dk, dl, dnd, dsign, dj
    DIM dfull                   ' draw_view is drawing every seat: the bank's lines once, after
    DIM #dw
    DIM #ct, #ch                ' rows 10-11's colours: text, highlight
    DIM deciding, info_log
    DIM blink, blink_n, blink_on, #blink_t
    DIM log_len, tk_pos, #tk_t
    DIM dmsg                    ' display_message's message (M_*)

    CONST M_DIALING  = 0
    CONST M_NOCARR   = 1
    CONST M_LOST     = 2
    CONST M_NOSIG    = 3
    CONST M_VERSION  = 4
    CONST M_NOFUJI   = 5

' ---------------------------------------------------------------- drawing

' put_ch: character dch at dp in colour #dc; dp moves on. "\" is the yen
' sign; lower case shows as upper case.
put_ch: PROCEDURE
    IF dch >= 96 THEN dch = dch - 32
    IF dch = 92 THEN #BACKTAB(dp) = G_YEN + #dc ELSE #BACKTAB(dp) = (dch - 32) * 8 + #dc
    dp = dp + 1
END

' put_sp: dn blanks.
put_sp: PROCEDURE
    WHILE dn
        #BACKTAB(dp) = #dc
        dp = dp + 1
        dn = dn - 1
    WEND
END

' put_ram: dn characters of cartridge RAM from #ds.
put_ram: PROCEDURE
    WHILE dn
        dch = rb(#ds)
        GOSUB put_ch
        #ds = #ds + 1
        dn = dn - 1
    WEND
END

' put_num: #dv right-aligned in dn columns, blanks before it.
put_num: PROCEDURE
    dk = dp + dn
    dl = dk
    DO
        dl = dl - 1
        #BACKTAB(dl) = (#dv % 10 + 16) * 8 + #dc
        #dv = #dv / 10
    LOOP WHILE #dv > 0 AND dl > dp
    dn = dl - dp
    GOSUB put_sp
    dp = dk
END

' put_money: #dv (in 100s) right-aligned in dn columns, "00" after it unless
' it is 0, the yen sign before it if dsign.
put_money: PROCEDURE
    #dw = #dv
    dnd = 1
    WHILE #dw >= 10
        #dw = #dw / 10
        dnd = dnd + 1
    WEND
    dl = dnd + dsign
    IF #dv THEN dl = dl + 2
    IF dn > dl THEN dn = dn - dl : GOSUB put_sp
    IF dsign THEN dch = 92 : GOSUB put_ch
    #dw = #dv
    dn = dnd
    GOSUB put_num
    IF #dw THEN #BACKTAB(dp) = 16 * 8 + #dc : #BACKTAB(dp + 1) = 16 * 8 + #dc : dp = dp + 2
END

' cash6: a seat's cash (#dv) in 6 columns: "\99900", then the digits alone
' up to 999900, then thousands and a K.
cash6: PROCEDURE
    dn = 6
    IF #dv < 1000 THEN dsign = 1 : GOSUB put_money : RETURN
    IF #dv < 10000 THEN dsign = 0 : GOSUB put_money : RETURN
    #dv = #dv / 10
    dn = 5
    GOSUB put_num
    dch = 75 : GOSUB put_ch
END

' node_name: node dnode as its column letter and row digit.
node_name: PROCEDURE
    dr = dnode / RULE_COLS
    dch = 65 + dnode - dr * RULE_COLS : GOSUB put_ch
    dch = 48 + dr : GOSUB put_ch
END

' clear_rows: dn cards of blank from dp (in #dc's background).
clear_rows: PROCEDURE
    #dc = #dc AND $3600         ' the background's bits
    GOSUB put_sp
END

' The keypad picks a syndicate here: dl = 1 if syndicate dj is an answer.
pick_ok: PROCEDURE
    dl = 0
    IF pr_kind = P_FOUND THEN IF rb(PR + 1) AND bit_of(dj) THEN dl = 1
    IF pr_kind = P_TIE THEN IF rb(PR + 2) AND bit_of(dj) THEN dl = 1
    IF pr_kind = P_BUY AND step = 0 THEN IF rb(PR + 4 + dj) THEN dl = 1
END

' ---------------------------------------------------------------- the Grid

' display_node: node dnode.
display_node: PROCEDURE
    IF field = 0 OR helping OR view <> V_GRID THEN RETURN
    dval = rb(BOARD + dnode)
    dr = dnode / RULE_COLS
    dp = dr * 20 + 21 + dnode - dr * RULE_COLS
    IF dval = 0 THEN
        dch = GL_EMPTY : #dc = CW_DIM
        IF hints THEN
            FOR dk = 0 TO RULE_KIT - 1
                dl = rb(KIT + dk)
                IF (dl AND $7F) = dnode THEN
                    dch = 49 + dk
                    IF dl AND $80 THEN #dc = CW_NORUN ELSE #dc = CW_RUN
                END IF
            NEXT dk
        END IF
    ELSEIF dval = CELL_SPIKE THEN
        dch = GL_SPIKE : #dc = CW_VALUE
    ELSE
        dch = syn_letter(dval - 2) + 32 : #dc = syn_cw(dval - 2)
    END IF
    IF dnode = last_node AND blink_on THEN #dc = CW_ICE
    GOSUB put_ch
END

' grid_mine: the panel's own lines: your crypto, your cash, the last move.
grid_mine: PROCEDURE
    IF pr_kind <> P_BUY THEN
        FOR dj = 0 TO NSYN - 1
            dp = dj * 20 + 38
            #dv = rb(COINS + me * NSYN + dj)
            IF #dv THEN #dc = CW_MINE : dn = 2 : GOSUB put_num ELSE #dc = CW_DIM : dch = 32 : GOSUB put_ch : dch = GL_EMPTY : GOSUB put_ch
        NEXT dj
    END IF
    dp = 174
    #dc = CW_MINE
    #dv = rb(CASHLO + me) + rb(CASHHI + me) * 256
    GOSUB cash6
    GOSUB last_line
END

' last_line: the panel's last line: the last move.
last_line: PROCEDURE
    IF field = 0 OR helping OR view <> V_GRID THEN RETURN
    dp = 193
    #dc = CW_TEXT
    IF last_node = NONE THEN
        dn = 7 : GOSUB put_sp
    ELSE
        PRINT AT dp COLOR CW_TEXT, "LAST " : dp = dp + 5   'fit 5
        #dc = CW_VALUE : dnode = last_node : GOSUB node_name
    END IF
END

' display_hives: the panel's syndicates (GRID), or the bank's lines (CRYPTO).
display_hives: PROCEDURE
    IF field = 0 OR helping THEN RETURN
    IF view = V_CRYPTO THEN GOSUB crypto_foot : RETURN
    IF view <> V_GRID THEN RETURN
    FOR dj = 0 TO NSYN - 1
        dp = dj * 20 + 33
        GOSUB pick_ok
        #dc = CW_RUN
        IF dl THEN dch = 49 + dj ELSE dch = 32
        GOSUB put_ch
        dch = syn_letter(dj) + 32 : #dc = syn_cw(dj) : GOSUB put_ch
        IF pr_kind = P_BUY THEN
            #dv = rb(PR + 4 + dj)
            dn = 5
            IF #dv THEN #dc = CW_VALUE : dsign = 1 : GOSUB put_money ELSE #dc = CW_DIM : dn = 4 : GOSUB put_sp : dch = 45 : GOSUB put_ch
        ELSE
            #dv = rb(HSIZE + dj)
            dn = 2
            IF #dv = 0 THEN
                #dc = CW_DIM : dch = 32 : GOSUB put_ch : dch = 45 : GOSUB put_ch
            ELSE
                IF #dv >= RULE_SAFE THEN #dc = CW_ICE ELSE #dc = CW_VALUE
                GOSUB put_num
            END IF
            #dc = CW_TEXT : dch = 32 : GOSUB put_ch
        END IF
    NEXT dj
    GOSUB grid_mine
END

' ---------------------------------------------------------------- seats

' seat_mark: seat dseat's marker: > you, ? away, * bot.
seat_mark: PROCEDURE
    dk = rb(SKIND + dseat)
    #dc = CW_TEXT : dch = 32
    IF dk = SEAT_AWAY THEN dch = 63
    IF dk = SEAT_BOT THEN dch = 42
    IF dseat = me THEN dch = 62 : #dc = CW_MINE
    GOSUB put_ch
END

' seat_name: dn letters of seat dseat's handle, inverse on its turn (and at
' the end, for whoever came top).
seat_name: PROCEDURE
    #dc = CW_VALUE
    IF dseat = turn THEN #dc = CW_INV
    IF turn = NONE THEN IF winners AND bit_of(dseat) THEN #dc = CW_INV
    #ds = SNAME + dseat * 8
    GOSUB put_ram
END

display_seat: PROCEDURE
    IF dseat >= SEATS OR field = 0 OR helping THEN RETURN
    IF view = V_GRID THEN
        IF dseat = me THEN GOSUB grid_mine
        RETURN
    END IF
    dp = dseat * 20 + 20
    GOSUB seat_mark
    IF rb(SKIND + dseat) = SEAT_EMPTY THEN
        dch = 45 : GOSUB put_ch
        dn = 18 : GOSUB put_sp
        RETURN
    END IF
    IF view = V_SEATS THEN
        dn = 8 : GOSUB seat_name
        #dc = CW_TEXT : dn = 5 : GOSUB put_sp
        #dc = CW_VALUE
        IF dseat = me THEN #dc = CW_MINE
        #dv = rb(CASHLO + dseat) + rb(CASHHI + dseat) * 256
        GOSUB cash6
        RETURN
    END IF
    dn = 5 : GOSUB seat_name
    FOR dj = 0 TO NSYN - 1
        #dv = rb(COINS + dseat * NSYN + dj)
        dn = 2
        IF #dv = 0 THEN
            #dc = CW_DIM : dch = 32 : GOSUB put_ch : dch = GL_EMPTY : GOSUB put_ch
        ELSE
            IF dj AND 1 THEN #dc = fgbg(C_TAN, C_BLACK) ELSE #dc = CW_VALUE
            IF dseat = me THEN #dc = CW_MINE
            GOSUB put_num
        END IF
    NEXT dj
    IF dfull = 0 THEN GOSUB crypto_foot     ' the bank's left changes with it
END

' crypto_foot: CRYPTO's rows 7-9: what the bank has left, the hives' sizes,
' and while buying their prices (in 100s).
crypto_foot: PROCEDURE
    PRINT AT 140 COLOR CW_TEXT, "LEFT  "                     'fit 6
    PRINT AT 160 COLOR CW_TEXT, "{T_HIVE}"                   'fit 6
    dp = 180 : #dc = CW_TEXT : dn = 6 : GOSUB put_sp
    IF pr_kind = P_BUY THEN PRINT AT 180 COLOR CW_TEXT, "PRICE"
    FOR dj = 0 TO NSYN - 1
        #dv = RULE_SUPPLY
        FOR dk = 0 TO SEATS - 1
            #dv = #dv - rb(COINS + dk * NSYN + dj)
        NEXT dk
        ' neighbouring numbers in two colours, so they stand apart
        IF dj AND 1 THEN #dw = fgbg(C_TAN, C_BLACK) ELSE #dw = CW_VALUE
        dp = 146 + dj * 2 : #dc = #dw : dn = 2 : GOSUB put_num
        #dv = rb(HSIZE + dj)
        dp = 166 + dj * 2 : dn = 2
        IF #dv = 0 THEN
            #dc = CW_DIM : dch = 32 : GOSUB put_ch : dch = 45 : GOSUB put_ch
        ELSE
            IF #dv >= RULE_SAFE THEN #dc = CW_ICE ELSE #dc = #dw
            GOSUB put_num
        END IF
        dp = 186 + dj * 2 : #dc = #dw : dn = 2
        IF pr_kind = P_BUY THEN #dv = rb(PR + 4 + dj) : GOSUB put_num ELSE GOSUB put_sp
    NEXT dj
END

' ---------------------------------------------------------------- the log

' display_log: MSG holds an "l" frame: keep its text for the ticker, and
' as 1 or 2 of SEATS' 3 log rows (wrapped at a word, the rest indented).
display_log: PROCEDURE
    log_len = rb(MSG + 1)
    IF log_len > 40 THEN log_len = 40
    CALL COPYB(MSG + 2, LOGLINE, log_len)
    dl = log_len
    dk = 0                      ' where the second row starts, 0 = one row
    IF log_len > 20 THEN
        dl = 20
        WHILE dl > 0 AND rb(LOGLINE + dl) <> 32
            dl = dl - 1
        WEND
        IF dl = 0 THEN dl = 20
        dk = dl
        IF rb(LOGLINE + dk) = 32 THEN dk = dk + 1
    END IF
    ' up a row or two
    IF dk THEN CALL COPYB(LOGROW + 40, LOGROW, 20) ELSE CALL COPYB(LOGROW + 20, LOGROW, 40)
    FOR dj = 0 TO 39
        IF dk OR dj >= 20 THEN POKE LOGROW + 20 + dj, 32
    NEXT dj
    IF dk THEN
        CALL COPYB(LOGLINE, LOGROW + 20, dl)
        dn = log_len - dk
        IF dn > 19 THEN dn = 19
        CALL COPYB(LOGLINE + dk, LOGROW + 41, dn)
    ELSE
        CALL COPYB(LOGLINE, LOGROW + 40, dl)
    END IF
    IF field AND helping = 0 AND view = V_SEATS THEN GOSUB seats_log
    tk_pos = 0
    #tk_t = FRAME
    IF info_log AND field AND helping = 0 THEN GOSUB ticker
END

seats_log: PROCEDURE
    dp = 140 : #ds = LOGROW : dn = 60 : #dc = CW_TEXT : GOSUB put_ram
END

' ticker: the newest log line on row 10, from tk_pos.
ticker: PROCEDURE
    dp = ROW_INFO
    #dc = #ct
    #ds = LOGLINE + tk_pos
    dn = log_len - tk_pos
    IF dn > 20 THEN dn = 20
    dl = 20 - dn
    GOSUB put_ram
    dn = dl
    GOSUB put_sp
END

' display_idle: every pass of the game loop: the last move's blink, the
' ticker.
display_idle: PROCEDURE
    IF field = 0 OR helping THEN RETURN
    IF last_node <> blink THEN
        blink = last_node
        IF blink <> NONE THEN blink_n = FLASH_FLIPS : blink_on = 1 : #blink_t = FRAME : dnode = blink : GOSUB display_node
    ELSEIF blink_n THEN
        IF FRAME - #blink_t >= FLASH_FRAMES THEN
            #blink_t = FRAME
            blink_n = blink_n - 1
            blink_on = blink_n AND 1
            dnode = blink : GOSUB display_node
        END IF
    END IF
    IF info_log AND log_len > 20 THEN
        IF tk_pos = 0 OR tk_pos = log_len - 20 THEN dk = TICK_PAUSE ELSE dk = TICK_FRAMES
        IF FRAME - #tk_t >= dk THEN
            #tk_t = FRAME
            tk_pos = tk_pos + 1
            IF tk_pos > log_len - 20 THEN tk_pos = 0
            GOSUB ticker
        END IF
    END IF
END

' ---------------------------------------------------------------- the views

' display_link: row 0: GRID's column letters, SEATS' region, CRYPTO's
' syndicates; NO SIGNAL in red while the link is down.
display_link: PROCEDURE
    IF field = 0 OR helping THEN RETURN
    IF link_up = 0 THEN
        dp = 0 : #dc = CW_ALERT : dn = 20 : GOSUB put_sp
        PRINT AT 1 COLOR CW_ALERT, "NO SIGNAL"
        RETURN
    END IF
    IF view = V_GRID THEN
        PRINT AT 0 COLOR CW_TEXT, " ABCDEFGHIJKL "
        PRINT AT 14 COLOR CW_TEXT, "{T_HIVE_P} "                  'fit 6
        RETURN
    END IF
    IF view = V_CRYPTO THEN
        dp = 0 : #dc = CW_BLANK : dn = 20 : GOSUB put_sp
        PRINT AT 0 COLOR CW_TEXT, "{T_COIN}"                    'fit 6
        FOR dj = 0 TO NSYN - 1
            dp = 7 + dj * 2 : GOSUB put_syn
        NEXT dj
        RETURN
    END IF
    dp = 0 : #dc = CW_INV : dn = 20 : GOSUB put_sp
    dp = 1 : #ds = REGION : dn = region_len : GOSUB put_ram
END

' draw_view: rows 0-9 of the view that's up.
draw_view: PROCEDURE
    dp = 0 : #dc = CW_BLANK : dn = 200 : GOSUB put_sp
    GOSUB display_link
    IF view = V_GRID THEN
        FOR dj = 0 TO RULE_ROWS - 1
            PRINT AT dj * 20 + 20 COLOR CW_TEXT, <>dj
        NEXT dj
        FOR dnode = 0 TO NODES - 1
            GOSUB display_node
        NEXT dnode
        GOSUB display_hives
    ELSEIF view = V_SEATS THEN
        FOR dseat = 0 TO SEATS - 1
            GOSUB display_seat
        NEXT dseat
        GOSUB seats_log
    ELSE
        dfull = 1
        FOR dseat = 0 TO SEATS - 1
            GOSUB display_seat
        NEXT dseat
        dfull = 0
        GOSUB crypto_foot
    END IF
END

' display_playfield: everything again, from the game state.
display_playfield: PROCEDURE
    IF field = 0 THEN GOSUB screen_game
    field = 1
    over_shown = 0
    IF helping THEN RETURN
    GOSUB draw_view
    GOSUB display_prompt
END

' next_view: the disc or the lower right button: dk = 1 on, 2 back.
next_view: PROCEDURE
    view = view + dk
    IF view > V_CRYPTO THEN view = view - 3
    GOSUB draw_view
END

' screen_game: foreground/background mode, the screen blank.
screen_game: PROCEDURE
    snow_on = 0
    dp = 0 : #dc = CW_BLANK : dn = 240 : GOSUB put_sp
    MODE 1
    BORDER C_BLACK
    scr_mode = 1
    WAIT
END

' Syndicates: letters (as IntyBASIC stores DATA text: ASCII - 32), colours
syn_letter:
    DATA "{SYN_LETTERS}"
syn_cw:
    DATA fgbg(C_BLACK, C_CYAN), fgbg(C_BLACK, C_TAN), fgbg(C_WHITE, C_PURPLE), fgbg(C_WHITE, C_BROWN)
    DATA fgbg(C_WHITE, C_RED), fgbg(C_BLACK, C_GREEN), fgbg(C_BLACK, C_ORANGE)
bit_of:
    DATA 1, 2, 4, 8, 16, 32, 64, 128
