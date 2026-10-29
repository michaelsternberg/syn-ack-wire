' ===========================================================================
' game.bas -- the game state from server frames, and keys into prompt
' answers: src/common/game.c in IntyBASIC. The server decides; the client
' checks an answer first so the player gets a reason at once (complaint),
' and an answer the server refuses comes back as a log line and the same
' prompt.
'
' The state is in cartridge RAM (memory.bas); display.bas reads it.
' ===========================================================================

    DIM gk                      ' game_key's key
    DIM sell                    ' a hivejack's crypto cashed out (its first answer)
    DIM seen

' game_frame: the frame in MSG (type pk), except "f", which main.bas takes.
game_frame: PROCEDURE
    ON pk - 97 GOSUB fr_a, , , , fr_e, , , fr_h, fr_i, , fr_k, fr_l, fr_m, fr_n, , fr_p, , fr_r, fr_s, fr_t, , , fr_w, fr_x, , fr_z
END

' w: seat, token
fr_w: PROCEDURE
    me = rb(MSG + 1)
    CALL COPYB(MSG + 2, TOKEN, 4)
END

' a: the region's name
fr_a: PROCEDURE
    n = rb(MSG + 1)
    IF n > 12 THEN n = 12
    region_len = n
    CALL COPYB(MSG + 2, REGION, n)
END

' i: the title's status: length, code, text (28 at most)
fr_i: PROCEDURE
    CALL COPYB(MSG + 1, STATUS, 30)
END

' n: seat, kind, handle[8]
fr_n: PROCEDURE
    dseat = rb(MSG + 1)
    IF dseat >= SEATS THEN RETURN
    POKE SKIND + dseat, rb(MSG + 2)
    CALL COPYB(MSG + 3, SNAME + dseat * 8, 8)
    GOSUB display_seat
END

' s: everything: the board (two nodes a byte, the even one low), the hives'
' sizes, the seats' cash and crypto, whose turn
fr_s: PROCEDURE
    j = 0
    FOR i = 1 TO NODES / 2
        b = rb(MSG + i)
        POKE BOARD + j, b AND 15
        POKE BOARD + j + 1, b / 16
        j = j + 2
    NEXT i
    CALL COPYB(MSG + 55, HSIZE, NSYN)
    FOR i = 0 TO SEATS - 1
        POKE CASHLO + i, rb(MSG + 62 + i * 2)
        POKE CASHHI + i, rb(MSG + 63 + i * 2)
    NEXT i
    CALL COPYB(MSG + 74, COINS, SEATS * NSYN)
    turn = rb(MSG + 116)
    last_node = NONE
    IF turn = NONE THEN             ' the lobby: no kit yet
        FOR i = 0 TO RULE_KIT - 1
            POKE KIT + i, NONE
        NEXT i
    END IF
    pr_kind = 0
    GOSUB display_playfield
END

' redraw_kit: the nodes the kit points at (their digits)
redraw_kit: PROCEDURE
    FOR i = 0 TO RULE_KIT - 1
        b = rb(KIT + i)
        IF b <> NONE THEN dnode = b AND $7F : GOSUB display_node
    NEXT i
END

toggle_hints: PROCEDURE
    hints = hints XOR 1
    GOSUB redraw_kit
END

' h: the kit
fr_h: PROCEDURE
    FOR i = 0 TO RULE_KIT - 1
        b = rb(KIT + i)
        POKE KIT + i, rb(MSG + 1 + i)
        IF b <> NONE THEN dnode = b AND $7F : GOSUB display_node    ' its old node loses its digit
    NEXT i
    GOSUB redraw_kit
    IF pr_kind = 0 THEN GOSUB display_prompt
END

' t: whose turn
fr_t: PROCEDURE
    n = turn
    turn = rb(MSG + 1)
    IF n < SEATS THEN dseat = n : GOSUB display_seat
    dseat = turn : GOSUB display_seat
    IF turn <> me AND pr_kind <> 0 AND pr_kind <> P_SEATS THEN
        GOSUB clear_prompt          ' our bot answered for us
    ELSEIF pr_kind = 0 THEN
        GOSUB display_prompt        ' WAITING FOR, or clear it on our turn
    END IF
END

' x: length, value, nodes: those nodes are now value
fr_x: PROCEDURE
    v = rb(MSG + 2)
    n = rb(MSG + 1) + 1
    IF v = CELL_SPIKE AND n = 3 THEN    ' a single new spike: the move just made
        b = last_node
        last_node = rb(MSG + 3)
        IF b <> NONE THEN dnode = b : GOSUB display_node
        GOSUB last_line
        sc = CHORD_SPIKE : GOSUB play_chord
    END IF
    i = 3
    WHILE i <= n
        b = rb(MSG + i)
        POKE BOARD + b, v
        dnode = b : GOSUB display_node
        i = i + 1
    WEND
END

' r: a hivejack: every node of value `from` becomes `to`
fr_r: PROCEDURE
    sc = CHORD_JACK : GOSUB play_chord
    v = rb(MSG + 1)
    b = rb(MSG + 2)
    FOR i = 0 TO NODES - 1
        IF rb(BOARD + i) = v THEN POKE BOARD + i, b : dnode = i : GOSUB display_node
    NEXT i
END

' z: syndicate, its hive's size
fr_z: PROCEDURE
    n = rb(MSG + 1)
    IF n >= NSYN THEN RETURN
    b = rb(HSIZE + n)
    v = rb(MSG + 2)
    POKE HSIZE + n, v
    IF b < RULE_SAFE AND v >= RULE_SAFE THEN
        sc = CHORD_ICE : GOSUB play_chord
    ELSEIF b = 0 AND v <> 0 THEN
        sc = CHORD_HIVE : GOSUB play_chord
    END IF
    IF pr_kind <> P_BUY THEN GOSUB display_hives
END

' m: seat, cash
fr_m: PROCEDURE
    dseat = rb(MSG + 1)
    IF dseat >= SEATS THEN RETURN
    POKE CASHLO + dseat, rb(MSG + 2)
    POKE CASHHI + dseat, rb(MSG + 3)
    GOSUB display_seat
END

' k: seat, syndicate, crypto held
fr_k: PROCEDURE
    dseat = rb(MSG + 1)
    IF dseat >= SEATS THEN RETURN
    POKE COINS + dseat * NSYN + rb(MSG + 2), rb(MSG + 3)
    GOSUB display_seat
END

' p: length, kind, timeout, params (to PR: the timeout, then the params)
fr_p: PROCEDURE
    pr_kind = rb(MSG + 2)
    n = rb(MSG + 1) - 1
    IF n > 18 THEN n = 18
    CALL COPYB(MSG + 3, PR, n)
    step = 0
    edit_len = 0
    complaint = 0
    secs = rb(PR)
    #t0 = FRAME
    IF pr_kind = P_BUY THEN
        IF rb(PR + 3) AND 2 THEN step = 1       ' nothing to buy: asked only because the game may end
        sc = CHORD_BUY : GOSUB play_chord
    ELSEIF pr_kind = P_SPIKE THEN
        sc = CHORD_TURN : GOSUB play_chord
    END IF
    IF view <> V_GRID THEN
        view = V_GRID
        IF field AND helping = 0 THEN GOSUB draw_view
    ELSE
        GOSUB display_hives                     ' the keypad's digits, or the prices
    END IF
    GOSUB display_prompt
END

' l: a log line
fr_l: PROCEDURE
    GOSUB display_log
END

' e: the winners
fr_e: PROCEDURE
    winners = rb(MSG + 1)
    n = turn
    turn = NONE                     ' back in the lobby
    IF n < SEATS THEN               ' not the replay of an old result
        dseat = n : GOSUB display_seat
        sc = CHORD_END : GOSUB play_chord
    END IF
    GOSUB clear_prompt
    GOSUB display_game_over
END

' clear_prompt: answered, or the clock ran out.
clear_prompt: PROCEDURE
    b = pr_kind
    pr_kind = 0
    complaint = 0
    IF b = P_BUY OR b = P_FOUND OR b = P_TIE THEN GOSUB display_hives
    GOSUB display_prompt
END

' ---------------------------------------------------------------- keys out

' send2: the prompt's letter and v.
send2: PROCEDURE
    POKE TX0, pr_kind
    POKE TX0 + 1, v
    n = 2 : GOSUB net_send
    GOSUB clear_prompt
END

' why_not: why exploit node b can't run (the server only marks it): next to
' two or more hives it would join two Black ICE hives; otherwise it would
' found a hive with every syndicate already on the Grid.
why_not: PROCEDURE
    seen = 0
    k = b AND $7F
    i = k / RULE_COLS               ' its row
    j = k - i * RULE_COLS           ' and column
    IF i > 0 THEN v = rb(BOARD + k - RULE_COLS) : GOSUB look
    IF i < RULE_ROWS - 1 THEN v = rb(BOARD + k + RULE_COLS) : GOSUB look
    IF j > 0 THEN v = rb(BOARD + k - 1) : GOSUB look
    IF j < RULE_COLS - 1 THEN v = rb(BOARD + k + 1) : GOSUB look
    IF seen AND (seen - 1) THEN complaint = W_JOINS ELSE complaint = W_NOSYN
    GOSUB display_prompt
END

look: PROCEDURE
    IF v >= 2 THEN seen = seen OR bit_of(v - 2)
END

' typed: the number typed so far (0 if nothing), in v.
typed: PROCEDURE
    v = 0
    FOR j = 0 TO 1
        IF j < edit_len THEN v = v * 10 + rb(EDIT + j) - 48
    NEXT j
END

' A hivejack's crypto: pr: timeout, absorbed, absorber, held, supply.
key_dispose: PROCEDURE
    IF gk >= 48 AND gk <= 57 AND edit_len < 2 THEN
        POKE EDIT + edit_len, gk
        edit_len = edit_len + 1
    ELSEIF gk = K_DELETE AND edit_len > 0 THEN
        edit_len = edit_len - 1
    ELSEIF gk = K_RETURN THEN
        GOSUB typed
        IF step = 0 THEN
            IF v > rb(PR + 3) THEN complaint = W_HOLD : GOTO kd_refuse
            sell = v
            step = 1
        ELSEIF v AND 1 THEN
            complaint = W_EVEN : GOTO kd_refuse
        ELSEIF sell + v > rb(PR + 3) THEN
            complaint = W_HOLD : GOTO kd_refuse
        ELSEIF v / 2 > rb(PR + 4) THEN
            complaint = W_POOL : GOTO kd_refuse
        ELSE
            POKE TX0, P_DISP
            POKE TX0 + 1, sell
            POKE TX0 + 2, v
            n = 3 : GOSUB net_send
            GOSUB clear_prompt
            RETURN
        END IF
        edit_len = 0
    ELSE
        RETURN
    END IF
    complaint = 0
kd_refuse:
    GOSUB display_prompt
END

' send_buy: the letters typed as syndicates (7 = none); v = 1 calls the
' null-route.
send_buy: PROCEDURE
    POKE TX0, P_BUY
    FOR i = 0 TO RULE_MAXBUY - 1
        dj = 7
        IF i < edit_len THEN dch = rb(EDIT + i) : GOSUB syn_index
        POKE TX0 + 1 + i, dj
    NEXT i
    IF v THEN POKE TX0 + 1, rb(TX0 + 1) OR $80
    n = 1 + RULE_MAXBUY : GOSUB net_send
    GOSUB clear_prompt
END

' A buy: pr: timeout, cash lo, hi, flags, price[7], supply[7].
key_buy: PROCEDURE
    IF step = 1 THEN                ' null-route?
        IF gk = 89 THEN v = 1 : GOSUB send_buy : RETURN
        IF gk = 78 OR gk = K_RETURN THEN v = 0 : GOSUB send_buy
        RETURN
    END IF
    IF gk = K_RETURN THEN
        IF rb(PR + 3) AND 1 THEN
            step = 1 : complaint = 0 : GOSUB display_prompt
        ELSE
            v = 0 : GOSUB send_buy
        END IF
        RETURN
    END IF
    IF gk = K_DELETE THEN
        IF edit_len THEN edit_len = edit_len - 1
        complaint = 0 : GOSUB display_prompt
        RETURN
    END IF
    dch = gk : GOSUB syn_index
    b = dj
    IF b = NONE THEN RETURN
    IF edit_len = RULE_MAXBUY THEN complaint = W_MAXBUY : GOSUB display_prompt : RETURN
    IF rb(PR + 4 + b) = 0 THEN complaint = W_NOHIVE : GOSUB display_prompt : RETURN
    n = 0
    #w = rb(PR + 1) + rb(PR + 2) * 256      ' and the money left after the letters so far
    FOR j = 0 TO RULE_MAXBUY - 1
        IF j < edit_len THEN
            IF rb(EDIT + j) = gk THEN n = n + 1
            dch = rb(EDIT + j) : GOSUB syn_index
            #w = #w - rb(PR + 4 + dj)
        END IF
    NEXT j
    IF n >= rb(PR + 4 + NSYN + b) THEN complaint = W_NONE : GOSUB display_prompt : RETURN
    IF #w < rb(PR + 4 + b) THEN complaint = W_CASH : GOSUB display_prompt : RETURN
    POKE EDIT + edit_len, gk
    edit_len = edit_len + 1
    complaint = 0
    GOSUB display_prompt
END

' game_key: key gk while a prompt is open.
game_key: PROCEDURE
    IF pr_kind = P_SEATS THEN
        IF gk >= 48 + RULE_MIN AND gk <= 48 + SEATS THEN v = gk - 48 : GOSUB send2
    ELSEIF pr_kind = P_SPIKE THEN
        n = gk - 49
        IF n >= RULE_KIT THEN RETURN
        b = rb(KIT + n)
        IF b = NONE THEN
            complaint = W_EMPTY : GOSUB display_prompt
        ELSEIF b AND $80 THEN
            GOSUB why_not
        ELSE
            v = b : GOSUB send2
        END IF
    ELSEIF pr_kind = P_FOUND OR pr_kind = P_TIE THEN
        dch = gk : GOSUB syn_index
        IF dj = NONE THEN RETURN
        IF pr_kind = P_FOUND THEN b = rb(PR + 1) ELSE b = rb(PR + 2)
        IF b AND bit_of(dj) THEN v = dj : GOSUB send2
    ELSEIF pr_kind = P_DISP THEN
        GOSUB key_dispose
    ELSEIF pr_kind = P_BUY THEN
        GOSUB key_buy
    END IF
END
