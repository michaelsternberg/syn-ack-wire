' ===========================================================================
' input.bas -- the hand controller and the ECS keyboard, as one stream of
' keys: in_key gives platform.h's (uppercase ASCII, K_RETURN, K_DELETE,
' K_ESC, K_HELP) and the INTV's own (K_NEXT and the disc's K_UP...).
'
' read_input runs in the frame handler and puts each press in the key ring
' (with the ECS keyboard's: ecskbd.bas), 16 keys, which in_key empties: a
' press made while the main loop waits on the cartridge, or draws a whole
' view, is kept, not lost. The decoding is M.U.L.E.'s
' (fujinet-multiplayer-mule, clients/intv/input.bas, GPL-3.0), the decoding
' every FujiNet Intellivision client shares. The controller's one byte
' carries the disc, the buttons and the keypad, and IntyBASIC's CONT1.*
' read each other's presses: count the row bits instead. None is the disc
' alone, one is a keypad key (no disc, no button, whatever the low bits
' say), two is a button (and the disc beside it).
'
' The disc gives a key when it is first pushed one of the four ways, and
' again every DISC_REPEAT frames while it is held past DISC_DELAY (the
' handle's dial).
'
'   keypad 0-9   the digit           CLEAR   K_DELETE      ENTER  K_RETURN
'   top buttons  K_HELP              lower left  K_ESC     lower right  K_NEXT
'   disc         K_UP, K_DOWN, K_LEFT, K_RIGHT
' ===========================================================================

    CONST CONT1_PORT = $01FF
    ' tools/intvsim.py drives the game headless: while TEST_HOOK holds $A5,
    ' the byte after it is the controller. Cleared at boot.
    CONST TEST_HOOK = $8FF0
    CONST INPUT_SETTLE = 1      ' frames the port must hold still (a key's row and column close apart)
    CONST DISC_DELAY = 20
    CONST DISC_REPEAT = 6

    ' The frame handler's
    DIM inp_raw, inp_new, inp_seen, inp_settle, inp_row
    DIM inp_dir, inp_btn, inp_key, inp_btn_prev, inp_key_prev
    DIM disc_now, disc_prev, disc_wait

' A keypad byte (one row bit) as the key: 0-9, 10 CLEAR, 11 ENTER, 12 none
' (two keys in a row). Read from the byte, not IntyBASIC's CONT1.KEY, so
' TEST_HOOK reaches it too. The keys are $81 1, $41 2, $21 3, $82 4, $42 5,
' $22 6, $84 7, $44 8, $24 9, $88 CLEAR, $48 0, $28 ENTER: the row bit $20,
' $40 or $80 picks a line of 16 (the byte / 64), the column bits the place.
    DEF FN keypad_of(r) = keypad_tab(((r) AND $0F) + ((r) / 64) * 16)
read_input: PROCEDURE
    inp_new = PEEK(CONT1_PORT) XOR 255
    IF rb(TEST_HOOK) = $A5 THEN inp_new = rb(TEST_HOOK + 1)
    IF inp_new <> inp_seen THEN
        inp_seen = inp_new
        inp_settle = INPUT_SETTLE
    ELSEIF inp_settle THEN
        inp_settle = inp_settle - 1
        IF inp_settle = 0 THEN inp_raw = inp_new
    END IF

    inp_row = inp_raw AND $E0
    inp_dir = 0
    inp_btn = 0
    inp_key = 12
    IF inp_row = $20 OR inp_row = $40 OR inp_row = $80 THEN
        inp_key = keypad_of(inp_raw)
    ELSE
        inp_btn = inp_row
        inp_dir = inp_raw AND $1F
    END IF

    IF inp_key <> inp_key_prev THEN
        inp_key_prev = inp_key
        IF inp_key < 10 THEN kq_k = 48 + inp_key : GOSUB kq_push
        IF inp_key = 10 THEN kq_k = K_DELETE : GOSUB kq_push
        IF inp_key = 11 THEN kq_k = K_RETURN : GOSUB kq_push
    END IF
    IF inp_btn <> inp_btn_prev THEN
        inp_btn_prev = inp_btn
        IF inp_btn = $A0 THEN kq_k = K_HELP : GOSUB kq_push
        IF inp_btn = $60 THEN kq_k = K_ESC : GOSUB kq_push
        IF inp_btn = $C0 THEN kq_k = K_NEXT : GOSUB kq_push
    END IF

    ' The disc, four ways: N K_UP, E K_RIGHT, S K_DOWN, W K_LEFT; the
    ' diagonals none.
    disc_now = disc4(inp_dir)
    IF disc_now <> disc_prev THEN
        disc_prev = disc_now
        IF disc_now THEN kq_k = disc_now : GOSUB kq_push : disc_wait = DISC_DELAY
    ELSEIF disc_now THEN
        disc_wait = disc_wait - 1
        IF disc_wait = 0 THEN kq_k = disc_now : GOSUB kq_push : disc_wait = DISC_REPEAT
    END IF
END

' kq_push: key kq_k into the ring (the frame handler only); a full ring drops it.
kq_push: PROCEDURE
    kq_n = (kq_tail + 1) AND KQ_MASK
    IF kq_n = kq_head THEN RETURN
    POKE KQ + kq_tail, kq_k
    kq_tail = kq_n
END

' in_key: the next key from the ring, or 0. The frame handler only writes
' kq_tail and this only kq_head, each with one instruction: no locking.
    DIM in_k
in_key: PROCEDURE
    in_k = 0
    IF kq_head = kq_tail THEN RETURN
    in_k = rb(KQ + kq_head)
    kq_head = (kq_head + 1) AND KQ_MASK
END

' The disc's 32 readings (the low 5 bits) as four keys
disc4:
    DATA 0, K_DOWN, K_RIGHT, K_DOWN, K_UP, 0, K_RIGHT, 0
    DATA K_LEFT, K_LEFT, 0, 0, K_UP, 0, 0, 0
    DATA 0, K_DOWN, K_RIGHT, 0, K_UP, 0, 0, 0
    DATA K_LEFT, 0, 0, 0, 0, 0, 0, 0

keypad_tab:
    DATA 12, 3, 6, 12, 9, 12, 12, 12, 11, 12, 12, 12, 12, 12, 12, 12
    DATA 12, 2, 5, 12, 8, 12, 12, 12, 0, 12, 12, 12, 12, 12, 12, 12
    DATA 12, 1, 4, 12, 7, 12, 12, 12, 10, 12, 12, 12, 12, 12, 12, 12
