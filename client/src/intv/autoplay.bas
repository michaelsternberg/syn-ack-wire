' ===========================================================================
' autoplay.bas -- the unattended build (make intv-auto): auto_key stands in
' for in_key, as test/autoplay.c does on the other platforms. It pages
' through the instructions, types a handle, then presses keys a player
' might for each prompt, wrong ones included, so the local checks and the
' server's refusals both get exercised. It presses the keypad's digits for
' syndicates and for yes and no, as a player would. When a new prompt
' opens, and every DUMP_FRAMES, it calls dump_hook, where tools/intvsim.py
' takes a picture.
' ===========================================================================

    CONST KEY_FRAMES  = 20      ' between keys: a third of a second
    CONST DUMP_FRAMES = 240

    DIM au_seed, au_typed, au_paged, au_shown, au_dumps
    DIM #au_key, #au_dump

auto_key: PROCEDURE
    GOSUB in_key                ' the real keys are read, and dropped
    in_k = 0
    IF rb(STATUS) = 0 AND rb(STATUS + 1) = 0 AND au_dumps = 0 THEN RETURN  ' the title, until the server answers
    IF au_dumps = 0 OR pr_kind <> au_shown OR FRAME - #au_dump >= DUMP_FRAMES THEN
        au_shown = pr_kind
        #au_dump = FRAME
        au_dumps = au_dumps + 1
        GOSUB dump_hook
    END IF
    IF FRAME - #au_key < KEY_FRAMES THEN RETURN
    #au_key = FRAME
    IF au_paged < 21 THEN       ' the instructions: open, page to the end and one past, back one, close
        in_k = auto_pages(au_paged)
        au_paged = au_paged + 1
        GOSUB dump_hook
        RETURN
    END IF
    IF au_typed < {#AUTO_HANDLE} + 1 THEN   ' the title: the handle, then ENTER
        IF au_typed = {#AUTO_HANDLE} THEN in_k = K_RETURN ELSE in_k = auto_name(au_typed) + 32
        au_typed = au_typed + 1
        RETURN
    END IF
    ' In a lobby with no question, the seats (the manual's capture of one)
    IF field <> 0 AND turn = NONE AND pr_kind = 0 AND view <> V_SEATS THEN in_k = K_NEXT : RETURN
    au_seed = au_seed * 2 XOR (au_seed AND $80) / 128 * $1D     ' an 8-bit LFSR
    IF au_seed = 0 THEN au_seed = $5A
    IF pr_kind = P_SEATS THEN in_k = 51
    IF pr_kind = P_SPIKE THEN in_k = 49 + (au_seed AND 7)        ' 7 and 8 are refused
    IF pr_kind = P_FOUND OR pr_kind = P_TIE THEN in_k = 49 + au_seed % NSYN
    IF pr_kind = P_DISP THEN
        IF au_seed AND 3 THEN in_k = 48 + (au_seed AND 3) ELSE in_k = K_RETURN
        IF complaint THEN in_k = K_DELETE       ' refused: start the number again
    END IF
    IF pr_kind = P_BUY THEN
        IF step THEN
            in_k = K_RETURN     ' never the null-route
        ELSEIF au_seed < 40 THEN
            in_k = K_DELETE
        ELSEIF au_seed < 100 THEN
            in_k = K_RETURN
        ELSE
            in_k = 49 + au_seed % NSYN
        END IF
    END IF
END

dump_hook: PROCEDURE
END

auto_pages:
    DATA K_HELP, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT
    DATA K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_RIGHT, K_LEFT, K_ESC, 0
auto_name:
    DATA "{AUTO_HANDLE}"
