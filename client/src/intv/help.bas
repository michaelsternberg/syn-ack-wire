' ===========================================================================
' help.bas -- the instruction pages, over the screen: help/intv/screen/
' HELP*.TXT, packed into the ROM by tools/help_nes.py as help_pages.bas,
' 12 lines of 20 each. A line's bytes: a character (+ 128: inverse), 1-15
' that many blanks, 0 its end; two to a word, the first in the high byte.
' The disc turns the pages; a button (or ENTER, CLEAR, ESC) goes back, and
' the screen that was up is drawn again.
' ===========================================================================

    DIM hp_page, hp_b, hp_row
    DIM #hp_at

help_open: PROCEDURE
    helping = 1
    IF scr_mode = 0 THEN GOSUB screen_game
    GOSUB help_draw
END

' help_key: key c for the pages; helping = 0 once they close.
help_key: PROCEDURE
    IF c = K_RIGHT OR c = K_DOWN OR c = 62 THEN
        IF hp_page < HELP_PAGES - 1 THEN hp_page = hp_page + 1 : GOSUB help_draw
        RETURN
    END IF
    IF c = K_LEFT OR c = K_UP OR c = 60 THEN
        IF hp_page THEN hp_page = hp_page - 1 : GOSUB help_draw
        RETURN
    END IF
    IF c = K_ESC OR c = K_HELP OR c = K_RETURN OR c = K_DELETE OR c = K_NEXT THEN helping = 0
END

help_draw: PROCEDURE
    #hp_at = help_page(hp_page)
    dp = 0
    FOR hp_row = 1 TO 12
        hp_b = 1
        WHILE hp_b
            #dw = help_data(#hp_at / 2)
            IF #hp_at AND 1 THEN hp_b = #dw AND 255 ELSE hp_b = #dw / 256
            #hp_at = #hp_at + 1
            IF hp_b = 0 THEN EXIT WHILE
            IF hp_b < 16 THEN
                #dc = CW_BLANK : dn = hp_b : GOSUB put_sp
            ELSEIF hp_b >= 128 THEN
                dch = hp_b - 128 : #dc = CW_INV : GOSUB put_ch
            ELSE
                dch = hp_b : #dc = CW_VALUE : GOSUB put_ch
            END IF
        WEND
        dn = hp_row * 20 - dp
        #dc = CW_BLANK
        GOSUB put_sp
    NEXT hp_row
END
