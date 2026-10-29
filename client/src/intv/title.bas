' ===========================================================================
' title.bas -- the title screen, in colour-stack mode: the Atari's logo
' (title_art.bas, from src/atari/title4.atascii) in colored squares, a
' square for each character of the art, its outline green and its letters'
' strokes static, rewritten a third at a time by the frame handler (SNOW);
' under it the region, the table's status, the handle and a line of keys.
'
'   rows 0-6   the logo
'   row 7      the region
'   rows 8-9   the status, wrapped at a word (or NO CARRIER)
'   row 10     HANDLE and the handle, the dial's place in yellow
'   row 11     a refusal, or the keys, one line at a time
'
' The handle: typed (the ECS, the keypad's digits), or dialled: the disc up
' and down changes the letter at the dial (past the end it starts a new
' one), right and left move the dial, CLEAR deletes.
' ===========================================================================

    CONST ROW_REGION = 140
    CONST ROW_STATUS = 160
    CONST ROW_HANDLE = 200
    CONST FOOT_FRAMES = 150     ' row 11's keys change every 2.5 s

    DIM dial                    ' the handle's letter the disc changes
    DIM foot, #foot_t

' display_title_screen: the whole title.
display_title_screen: PROCEDURE
    field = 0
    snow_on = 0
    IF scr_mode THEN
        dp = 0 : #dc = 0 : dn = 240 : GOSUB put_sp
        MODE 0, C_BLACK, C_BLACK, C_BLACK, C_BLACK
        BORDER C_BLACK
        scr_mode = 0
        WAIT
    END IF
    dp = 0 : #dc = 0 : dn = 240 : GOSUB put_sp
    dk = 0
    WHILE dk < title_logo(0)
        #BACKTAB(title_logo(dk * 2 + 1)) = title_logo(dk * 2 + 2)
        dk = dk + 1
    WEND
    snow_on = 1
    foot = 0
    #foot_t = FRAME
    GOSUB display_title_update
END

' display_title_update: its changing lines, drawn over in place.
display_title_update: PROCEDURE
    IF helping THEN RETURN
    dp = ROW_REGION : #dc = 0 : dn = 100 : GOSUB put_sp
    dp = ROW_REGION + (20 - region_len) / 2 : #ds = REGION : dn = region_len : #dc = C_WHITE : GOSUB put_ram

    dl = rb(STATUS)
    IF dl THEN
        dl = dl - 1             ' the text's length, from STATUS + 2
        dk = dl
        IF dl > 20 THEN         ' wrapped at a word
            dk = 20
            WHILE dk > 0 AND rb(STATUS + 2 + dk) <> 32
                dk = dk - 1
            WEND
            IF dk = 0 THEN dk = 20
        END IF
        dp = ROW_STATUS : #ds = STATUS + 2 : dn = dk : #dc = C_GREEN : GOSUB put_ram
        IF rb(STATUS + 2 + dk) = 32 THEN dk = dk + 1
        IF dl > dk THEN
            dn = dl - dk
            IF dn > 20 THEN dn = 20
            dp = ROW_STATUS + 20 : #ds = STATUS + 2 + dk : GOSUB put_ram
        END IF
    ELSEIF rb(STATUS + 1) = NO_CARRIER THEN
        PRINT AT ROW_STATUS COLOR C_RED, "NO CARRIER"
    END IF

    IF opting THEN GOSUB title_options : RETURN
    PRINT AT ROW_HANDLE COLOR C_TAN, "HANDLE "
    dp = ROW_HANDLE + 7
    FOR dk = 0 TO 7
        dch = 32 : #dc = C_WHITE
        IF dk < edit_len THEN dch = rb(EDIT + dk)
        IF dk = dial THEN
            #dc = C_YELLOW
        END IF
        IF dk = dial AND dk >= edit_len THEN
            #BACKTAB(dp) = G_LINE + C_YELLOW : dp = dp + 1      ' the next letter's place
        ELSE
            GOSUB put_ch
        END IF
    NEXT dk
    GOSUB title_foot
END

' title_foot: row 11.
title_foot: PROCEDURE
    dp = ROW_ASK : #dc = 0 : dn = 20 : GOSUB put_sp
    IF refused = REFUSED_LATE THEN PRINT AT ROW_ASK COLOR C_RED, "TOO LATE: SEAT TAKEN" : RETURN     'fit 20
    IF refused = REFUSED_HANDLE THEN PRINT AT ROW_ASK COLOR C_RED, "THAT HANDLE IS TAKEN" : RETURN   'fit 20
    IF foot = 0 THEN PRINT AT ROW_ASK COLOR C_YELLOW, "ENTER" : PRINT COLOR C_GREEN, ": {T_JACKIN}"   'fit 15
    IF foot = 1 THEN PRINT AT ROW_ASK COLOR C_YELLOW, "TOP BUTTON" : PRINT COLOR C_GREEN, ": HELP"
    IF foot = 2 THEN PRINT AT ROW_ASK COLOR C_YELLOW, "LOWER LEFT" : PRINT COLOR C_GREEN, ": OPTIONS"
    IF foot = 3 THEN PRINT AT ROW_ASK COLOR C_YELLOW, "DISC" : PRINT COLOR C_GREEN, ": DIAL A HANDLE"
END

' title_options: rows 10-11 while the options are up (Esc).
title_options: PROCEDURE
    dp = ROW_HANDLE : #dc = 0 : dn = 40 : GOSUB put_sp
    #ct = C_GREEN : #ch = C_YELLOW
    GOSUB options_rows
END

' display_title_idle: every pass of the title's loop: row 11's keys.
display_title_idle: PROCEDURE
    IF helping OR opting OR refused THEN RETURN
    IF FRAME - #foot_t < FOOT_FRAMES THEN RETURN
    #foot_t = FRAME
    foot = foot + 1
    IF foot = 4 THEN foot = 0
    GOSUB title_foot
END

' title_key: a key for the handle (c). Returns with c = 0 if it was used.
title_key: PROCEDURE
    IF c = K_UP OR c = K_DOWN THEN
        IF dial >= edit_len THEN
            IF edit_len = 8 THEN c = 0 : RETURN
            dial = edit_len
            edit_len = edit_len + 1
            IF c = K_UP THEN POKE EDIT + dial, 65 ELSE POKE EDIT + dial, 90
        ELSE
            dk = rb(EDIT + dial)
            ' A-Z, 0-9, ! to /, : to @, and round
            IF c = K_UP THEN
                IF dk = 90 THEN
                    dk = 48
                ELSEIF dk = 57 THEN
                    dk = 33
                ELSEIF dk = 47 THEN
                    dk = 58
                ELSEIF dk = 64 THEN
                    dk = 65
                ELSE
                    dk = dk + 1
                END IF
            ELSE
                IF dk = 65 THEN
                    dk = 64
                ELSEIF dk = 58 THEN
                    dk = 47
                ELSEIF dk = 33 THEN
                    dk = 57
                ELSEIF dk = 48 THEN
                    dk = 90
                ELSE
                    dk = dk - 1
                END IF
            END IF
            POKE EDIT + dial, dk
        END IF
    ELSEIF c = K_RIGHT THEN
        IF dial < edit_len AND dial < 7 THEN dial = dial + 1
    ELSEIF c = K_LEFT THEN
        IF dial THEN dial = dial - 1
    ELSEIF c = K_DELETE THEN
        IF edit_len = 0 THEN c = 0 : RETURN
        IF dial >= edit_len THEN dial = edit_len - 1
        dk = dial
        WHILE dk < edit_len - 1
            POKE EDIT + dk, rb(EDIT + dk + 1)
            dk = dk + 1
        WEND
        edit_len = edit_len - 1
    ELSEIF c > 32 AND c <= 90 THEN
        IF edit_len = 8 THEN c = 0 : RETURN
        POKE EDIT + edit_len, c
        edit_len = edit_len + 1
        dial = edit_len
        IF dial = 8 THEN dial = 7
    ELSE
        RETURN
    END IF
    c = 0
    GOSUB display_title_update
END

' SNOW(index, part): a third of the static: for each card of the part,
' its word with the random table's bits where the static is. Returns the
' next index into the random table.
    ASM SNOW:   PROC
    ASM         PSHR R5
    ASM         MOVR R1, R4
    ASM @@card: MVI@ R4, R1             ; its BACKTAB address; 0 ends
    ASM         TSTR R1
    ASM         BEQ  @@done
    ASM         MVI@ R4, R2             ; the card without static
    ASM         MVI@ R4, R3             ; the static's bits
    ASM         ADDI #73, R0            ; 73 is odd: every word of the table in turn
    ASM         ANDI #255, R0
    ASM         MOVR R0, R5
    ASM         ADDI #label_SNOW_RAND, R5
    ASM         AND@ R5, R3
    ASM         XORR R2, R3
    ASM         MVO@ R3, R1
    ASM         B    @@card
    ASM @@done: PULR PC
    ASM         ENDP

    INCLUDE "title_art.bas"
