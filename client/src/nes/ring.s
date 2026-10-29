; ring.s -- rx_ring for the NES: the RING segment, page $0300 of the console's
; RAM (game.cfg), so it starts on a page boundary and the 8-bit head and
; tail indexes wrap for free.
        .export _rx_ring

.segment "RING"
_rx_ring: .res 256
