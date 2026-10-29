' ===========================================================================
' prompt.bas -- the display's rows 10 and 11, the same in every view
' (display.bas has the rest): what the open prompt is about, a refusal or
' the newest log line on row 10, the question and the seconds left on row
' 11, purple while this seat has a decision to make; the options over both;
' the messages while dialing; who came top.
' ===========================================================================

' syn_index: the syndicate whose letter is dch (ASCII): dj, or NONE.
syn_index: PROCEDURE
    dj = 0
    WHILE dj < NSYN
        IF syn_letter(dj) + 32 = dch THEN RETURN
        dj = dj + 1
    WEND
    dj = NONE
END

' The info row's lines that name syndicates: letter dj in its colours.
put_syn: PROCEDURE
    dch = syn_letter(dj) + 32 : #dc = syn_cw(dj) : GOSUB put_ch
END

' kit_line: the six exploits, 3 cards each on alternating backgrounds: the
' digit, then the node, red if it can't run now.
kit_line: PROCEDURE
    dp = ROW_INFO
    FOR dj = 0 TO RULE_KIT - 1
        IF dj AND 1 THEN #dw = fgbg(C_BLACK, C_BLUE) - fgbg(C_BLACK, C_BLACK) ELSE #dw = 0
        #dc = CW_RUN + #dw : dch = 49 + dj : GOSUB put_ch
        dl = rb(KIT + dj)
        IF dl = NONE THEN
            #dc = CW_DIM + #dw : dch = 45 : GOSUB put_ch : GOSUB put_ch
        ELSE
            IF dl AND $80 THEN #dc = CW_NORUN + #dw ELSE #dc = CW_VALUE + #dw
            dnode = dl AND $7F : GOSUB node_name
        END IF
    NEXT dj
    #dc = #ct : dn = 2 : GOSUB put_sp
END

' The refusals (complaint), each at most 20
complaint_line: PROCEDURE
    IF complaint = W_HOLD THEN PRINT AT ROW_INFO COLOR #ch, "MORE THAN YOU HOLD"                'fit 20
    IF complaint = W_EVEN THEN PRINT AT ROW_INFO COLOR #ch, "{T_SWAP} AN EVEN NUMBER"           'fit 20
    IF complaint = W_POOL THEN PRINT AT ROW_INFO COLOR #ch, "{T_BANK} HAS TOO FEW"              'fit 20
    IF complaint = W_MAXBUY THEN PRINT AT ROW_INFO COLOR #ch, "{RULE_MAXBUY} PER TURN: ENTER"   'fit 20
    IF complaint = W_NOHIVE THEN PRINT AT ROW_INFO COLOR #ch, "NO {T_HIVE} ON THE {T_GRID}"     'fit 20
    IF complaint = W_NONE THEN PRINT AT ROW_INFO COLOR #ch, "NONE LEFT TO BUY"                  'fit 20
    IF complaint = W_CASH THEN PRINT AT ROW_INFO COLOR #ch, "NOT ENOUGH {T_MONEY}"              'fit 20
    IF complaint = W_EMPTY THEN PRINT AT ROW_INFO COLOR #ch, "THAT SLOT IS EMPTY"               'fit 20
    IF complaint = W_JOINS THEN PRINT AT ROW_INFO COLOR #ch, "JOINS 2 {T_SAFE}"                 'fit 20
    IF complaint = W_NOSYN THEN PRINT AT ROW_INFO COLOR #ch, "NO SYNDICATE LEFT"                'fit 20
END

' The options, over rows 10-11 (Esc): hints, sound, log off.
options_rows: PROCEDURE
    PRINT AT ROW_INFO COLOR #ch, "1"
    PRINT COLOR #ct, "HINTS "                                'fit 6
    IF hints THEN PRINT "ON " ELSE PRINT "OFF"
    PRINT AT ROW_INFO + 10 COLOR #ch, "2"
    PRINT COLOR #ct, "SOUND "                                'fit 6
    IF snd_on THEN PRINT "ON " ELSE PRINT "OFF"
    IF field THEN
        PRINT AT ROW_ASK COLOR #ch, "3"
        PRINT COLOR #ct, "{T_LOGOFF}"                        'fit 9
    END IF
    PRINT AT ROW_ASK + 9 COLOR #ct, "ENTER: BACK"            'fit 11
END

' display_prompt: rows 10-11 for the open prompt (or none).
display_prompt: PROCEDURE
    IF field = 0 OR helping THEN RETURN
    deciding = 0
    IF pr_kind OR turn = me OR leaving OR opting THEN deciding = 1
    IF deciding THEN #ct = CW_ASK : #ch = CW_ASKHI ELSE #ct = CW_TEXT : #ch = CW_MINE
    dp = ROW_INFO : #dc = #ct : dn = 40 : GOSUB clear_rows
    info_log = 0
    IF opting THEN GOSUB options_rows : RETURN

    ' row 10
    IF complaint THEN
        GOSUB complaint_line
    ELSEIF pr_kind = P_SPIKE THEN
        GOSUB kit_line
    ELSEIF pr_kind = P_FOUND THEN
        PRINT AT ROW_INFO COLOR #ct, "{T_INFILTRATION}!"          'fit 20
    ELSEIF pr_kind = P_TIE THEN
        PRINT AT ROW_INFO COLOR #ct, "TIE!"
    ELSEIF pr_kind = P_DISP THEN
        dp = ROW_INFO : dj = rb(PR + 2) : GOSUB put_syn
        PRINT AT dp COLOR #ct, " TAKES "
        dp = dp + 7 : dj = rb(PR + 1) : GOSUB put_syn
        PRINT AT dp COLOR #ct, ". HOLD "
        dp = dp + 7 : #dv = rb(PR + 3) : #dc = #ch : dn = 1
        IF #dv >= 10 THEN dn = 2
        GOSUB put_num
    ELSEIF pr_kind = P_BUY THEN
        IF step = 0 OR edit_len THEN
            PRINT AT ROW_INFO COLOR #ct, "BUY {RULE_MAXBUY} {T_COIN}: "   'fit 14
            dp = ROW_INFO + 14
            dk = 0
            WHILE dk < edit_len
                dch = rb(EDIT + dk) : GOSUB syn_index
                GOSUB put_syn
                dk = dk + 1
            WEND
        END IF
    ELSEIF over_shown AND (pr_kind = 0 OR pr_kind = P_SEATS) THEN
        GOSUB over_line
    ELSE
        info_log = 1
        GOSUB ticker
    END IF

    ' row 11
    IF leaving THEN
        PRINT AT ROW_ASK COLOR #ct, "{T_LOGOFF}? "                  'fit 13
        PRINT COLOR #ch, "1:YES"
    ELSEIF pr_kind = 0 THEN
        IF turn < SEATS AND turn <> me THEN
            PRINT AT ROW_ASK COLOR #ct, "WAITING FOR "              'fit 12
            dp = ROW_ASK + 12 : dseat = turn : #ds = SNAME + turn * 8 : dn = 8 : #dc = #ct : GOSUB put_ram
        END IF
    ELSEIF pr_kind = P_SEATS THEN
        PRINT AT ROW_ASK COLOR #ct, "SEATS? "
        PRINT COLOR #ch, "{RULE_MINPLAYERS}-{RULE_MAXPLAYERS}"
    ELSEIF pr_kind = P_SPIKE THEN
        PRINT AT ROW_ASK COLOR #ct, "RUN {T_EXPLOIT}? "             'fit 13
        PRINT COLOR #ch, "1-{RULE_KITSIZE}"
    ELSEIF pr_kind = P_FOUND THEN
        PRINT AT ROW_ASK COLOR #ct, "{T_BROKER} TO WHICH?"          'fit 17
    ELSEIF pr_kind = P_TIE THEN
        IF rb(PR + 1) THEN PRINT AT ROW_ASK COLOR #ct, "ABSORB WHICH NOW?" ELSE PRINT AT ROW_ASK COLOR #ct, "WHICH SURVIVES?"
    ELSEIF pr_kind = P_DISP THEN
        IF step THEN
            PRINT AT ROW_ASK COLOR #ct, "{T_SWAP} 2 FOR 1? "         'fit 15
            dp = ROW_ASK + 14
        ELSE
            PRINT AT ROW_ASK COLOR #ct, "{T_CASHOUT}? "              'fit 15
            dp = ROW_ASK + 10
        END IF
        #dc = #ch
        IF edit_len THEN #ds = EDIT : dn = edit_len : GOSUB put_ram ELSE dch = 48 : GOSUB put_ch
    ELSEIF pr_kind = P_BUY THEN
        IF step THEN
            PRINT AT ROW_ASK COLOR #ct, "{T_NULLROUTE}? "           'fit 12
            PRINT COLOR #ch, "1:YES"
        ELSE
            PRINT AT ROW_ASK COLOR #ch, "1-7"
            PRINT COLOR #ct, ", THEN "
            PRINT COLOR #ch, "ENTER"
        END IF
    END IF
    GOSUB clock
END

' display_clock: the seconds left, at the end of row 11: one short, as on
' the Apple, so it never promises more time than the server gives.
clock: PROCEDURE
    IF pr_kind = 0 OR opting OR field = 0 OR helping THEN RETURN
    dp = ROW_ASK + COL_CLOCK : dn = 2
    IF secs THEN #dv = secs - 1 : #dc = #ct : GOSUB put_num ELSE #dc = #ct : GOSUB put_sp
END

' display_message: message dmsg on row 11 (rows 10-11 for the version's),
' on the title or the game.
display_message: PROCEDURE
    IF field THEN #dc = CW_ALERT ELSE #dc = C_WHITE
    dp = ROW_INFO : dn = 40
    IF field THEN GOSUB clear_rows ELSE #dc = 0 : GOSUB put_sp : #dc = C_WHITE
    IF dmsg = M_DIALING THEN PRINT AT ROW_ASK COLOR #dc, "DIALING..."
    IF dmsg = M_NOCARR THEN PRINT AT ROW_ASK COLOR #dc, "NO CARRIER: REDIAL"        'fit 20
    IF dmsg = M_LOST THEN PRINT AT ROW_ASK COLOR #dc, "CARRIER LOST: REDIAL"        'fit 20
    IF dmsg = M_NOSIG THEN PRINT AT ROW_ASK COLOR #dc, "NO SIGNAL: REDIAL"          'fit 20
    IF dmsg = M_VERSION THEN PRINT AT ROW_INFO COLOR #dc, "SERVER SPEAKS" : PRINT AT ROW_ASK COLOR #dc, "ANOTHER VERSION"
    IF dmsg = M_NOFUJI THEN PRINT AT ROW_INFO COLOR #dc, "NO FUJINET CARTRIDGE" : PRINT AT ROW_ASK COLOR #dc, "FOUND"
END

' over_line: the top netrunner(s) on row 10: as many handles as fit.
over_line: PROCEDURE
    PRINT AT ROW_INFO COLOR #ct, "TOP:"
    dp = ROW_INFO + 4
    FOR dseat = 0 TO SEATS - 1
        IF winners AND bit_of(dseat) THEN
            dn = 8
            WHILE dn > 0 AND rb(SNAME + dseat * 8 + dn - 1) = 32
                dn = dn - 1
            WEND
            IF dp + 1 + dn <= ROW_INFO + 20 THEN
                #dc = #ct : dch = 32 : GOSUB put_ch
                #ds = SNAME + dseat * 8 : #dc = #ch : GOSUB put_ram
            END IF
        END IF
    NEXT dseat
END

' display_game_over: winners are in `winners`.
display_game_over: PROCEDURE
    over_shown = 1
    IF field = 0 OR helping THEN RETURN
    GOSUB draw_view
    GOSUB display_prompt
END

