' ===========================================================================
' net.bas -- platform.h's network calls for the Intellivision: one TCP
' connection through the FujiNet cartridge's mailbox (fujinet.bas), which
' the game never waits on while it plays.
'
' One mailbox transaction is out at a time. net_poll, once a frame from the
' main loop, looks for its answer and posts the next: the frames queued by
' net_send first, else a STATUS (at once while bytes arrive or an answer is
' due, else every POLL_FRAMES), and after a STATUS that says bytes wait, a
' READ of as many as fit: the mailbox's 512 at most, and no more than the
' 1K ring in cartridge RAM has room for. proto.bas takes whole frames from
' the ring. The open, the close and the AppKey reads wait for their answers:
' they come before play, or after it, and net_quiet first lets the
' transaction in flight end.
'
' net_err: 0 while the link is up, else why it went down (NET_FAIL: the
' cartridge didn't answer, or refused; NET_CLOSED: FujiNet says the
' connection is closed). It stays set until the next open.
' ===========================================================================

    CONST NX_STATUS   = 1
    CONST NX_READ     = 2
    CONST NX_WRITE    = 3
    CONST POLL_FRAMES = 6       ' about 10 STATUS a second while nothing comes
    CONST NET_FAIL    = 1
    CONST NET_CLOSED  = 255

    DIM nx_st                   ' the transaction out: 0 none, NX_*
    DIM net_err
    DIM flowing                 ' bytes came last time, or an answer is due: ask at once
    DIM txq_len
    DIM #rx_head, #rx_tail      ' bytes taken from the ring, and put in, so far
    DIM #asked                  ' FRAME of the last STATUS
    DIM #nx_n, #nx_a, #nx_k

' net_poll: a step of the network, once a frame.
net_poll: PROCEDURE
    IF nx_st = 0 THEN GOTO np_post
    GOSUB fn_check
    IF fn_ok = 255 THEN RETURN
    IF fn_ok = 0 THEN nx_st = 0 : net_err = NET_FAIL : RETURN
    IF nx_st = NX_STATUS THEN
        nx_st = 0
        #nx_n = rb(FN_RX) + rb(FN_RX + 1) * 256
        IF #nx_n = 0 THEN
            flowing = 0
            IF rb(FN_RX + 2) = 0 THEN net_err = NET_CLOSED
            RETURN
        END IF
        flowing = 1
        IF #nx_n > 512 THEN #nx_n = 512
        #nx_k = RING_MASK - (#rx_tail - #rx_head)
        IF #nx_n > #nx_k THEN #nx_n = #nx_k
        IF #nx_n = 0 THEN RETURN        ' the ring is full: frames are taken first
        mb_dev = NET_DEVICEID : mb_cmd = NETCMD_READ : mb_nparam = 1
        pm_i = 0 : pm_size = 2 : #pm_val = #nx_n : GOSUB fn_param
        #fn_txlen = 0
        GOSUB fn_post
        nx_st = NX_READ
        RETURN
    END IF
    IF nx_st = NX_READ THEN
        nx_st = 0
        #nx_n = rb(FN_RXLEN_LO) + rb(FN_RXLEN_HI) * 256
        IF #nx_n = 0 THEN net_err = NET_CLOSED : RETURN
        #nx_a = #rx_tail AND RING_MASK
        #nx_k = RING_MASK + 1 - #nx_a   ' room to the ring's end
        IF #nx_k > #nx_n THEN #nx_k = #nx_n
        CALL COPYB(FN_RX, RING + #nx_a, #nx_k)
        IF #nx_n > #nx_k THEN CALL COPYB(FN_RX + #nx_k, RING, #nx_n - #nx_k)
        #rx_tail = #rx_tail + #nx_n
        RETURN
    END IF
    nx_st = 0                           ' a WRITE went out
    RETURN

np_post:
    IF txq_len THEN
        CALL COPYB(TXQ, FN_TX, txq_len)
        mb_dev = NET_DEVICEID : mb_cmd = NETCMD_WRITE : mb_nparam = 1
        pm_i = 0 : pm_size = 2 : #pm_val = txq_len : GOSUB fn_param
        #fn_txlen = txq_len
        txq_len = 0
        GOSUB fn_post
        nx_st = NX_WRITE
        flowing = 1                     ' the answer is on its way: look at once
        RETURN
    END IF
    IF flowing = 0 AND FRAME - #asked < POLL_FRAMES THEN RETURN
    #asked = FRAME
    GOSUB fn_status_post
    nx_st = NX_STATUS
END

' net_send: queue the n bytes at TX0 (platform.h's tx[]) to go out.
net_send: PROCEDURE
    IF txq_len + n > 32 THEN RETURN  ' can't happen: the main loop sends once a frame at most
    CALL COPYB(TX0, TXQ + txq_len, n)
    txq_len = txq_len + n
END

' net_quiet: let the transaction out end (its answer is dropped).
net_quiet: PROCEDURE
    WHILE nx_st
        WAIT
        GOSUB fn_check
        IF fn_ok <> 255 THEN nx_st = 0
    WEND
END

' net_open: dial the devicespec staged in the TX buffer (#fn_txlen bytes).
' net_err = 0 if it answered: a failed open can still be ACKed, the failure
' shows in the status after it (not connected), as on the other platforms.
net_open: PROCEDURE
    GOSUB net_quiet
    #rx_head = 0 : #rx_tail = 0 : txq_len = 0 : flowing = 0
    net_err = NET_FAIL
    GOSUB fn_open
    IF fn_ok = 0 THEN RETURN
    GOSUB fn_status_post
    DO
        WAIT
        GOSUB fn_check
    LOOP WHILE fn_ok = 255
    IF fn_ok = 0 THEN RETURN
    IF rb(FN_RX + 2) = 0 THEN net_err = NET_CLOSED : RETURN
    net_err = 0
    #asked = FRAME
END

net_close: PROCEDURE
    GOSUB net_quiet
    GOSUB fn_close
    txq_len = 0
END

' net_appkey: AppKey ak_key into AKBUF + 2, as fujinet.bas reads it; fn_len
' its length. The very first read can come too soon after power-up: it gets
' a second try.
    DIM ak_tried
net_appkey: PROCEDURE
    GOSUB net_quiet
    GOSUB appkey_read
    IF fn_ok = 0 AND ak_tried = 0 THEN GOSUB appkey_read
    ak_tried = 1
END

' ---------------------------------------------------------------------------
' COPYB(src, dst, count): copy count bytes, masked to 8 bits (the mailbox's
' locations have other bits set). From M.U.L.E.'s net.bas. A PEEK/POKE loop
' is several times slower.
' ---------------------------------------------------------------------------
    ASM COPYB: PROC
    ASM     PSHR R5
    ASM     MOVR R0, R4
    ASM     MOVR R1, R5
    ASM     TSTR R2
    ASM     BEQ @@done
    ASM @@loop:
    ASM     MVI@ R4, R3
    ASM     ANDI #255, R3
    ASM     MVO@ R3, R5
    ASM     DECR R2
    ASM     BNE @@loop
    ASM @@done:
    ASM     PULR PC
    ASM     ENDP
