' ===========================================================================
' proto.bas -- the frame assembler: the bytes in the ring (net.bas) into one
' whole frame at a time in MSG, as src/common/proto.c does it.
'
' MSG holds the type letter, then the payload. A variable-length frame (a i
' l p x) keeps its length byte at MSG + 1, so its payload starts at MSG + 2.
' ===========================================================================

    DIM pn
    DIM #pa

' proto_next: the next whole frame into MSG; pk = its type, 0 if no whole
' frame has come yet, PROTO_BAD if the stream made no sense (the ring is
' flushed: send R, and the server resends everything).
proto_next: PROCEDURE
    pk = 0
    #pa = #rx_tail - #rx_head
    IF #pa = 0 THEN RETURN
    pn = rb(RING + (#rx_head AND RING_MASK)) - 97
    IF pn < 26 THEN pn = len_tab(pn) ELSE pn = LEN_BAD
    IF pn = LEN_VAR THEN
        IF #pa < 2 THEN RETURN
        pn = rb(RING + ((#rx_head + 1) AND RING_MASK))
        IF pn >= 127 THEN pn = LEN_BAD ELSE pn = pn + 1
    END IF
    IF pn = LEN_BAD THEN #rx_head = #rx_tail : pk = PROTO_BAD : RETURN
    IF #pa <= pn THEN RETURN            ' the type byte and pn more
    #pa = #rx_head AND RING_MASK
    #v = RING_MASK + 1 - #pa            ' to the ring's end
    #w = pn + 1
    IF #v > #w THEN #v = #w
    CALL COPYB(RING + #pa, MSG, #v)
    IF #w > #v THEN CALL COPYB(RING, MSG + #v, #w - #v)
    #rx_head = #rx_head + #w
    pk = rb(MSG)
END

' The payload length of each server frame type, from "a" (proto_tab.h)
len_tab:
    DATA {LEN_TAB}
