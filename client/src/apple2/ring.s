; ring.s -- rx_ring, page-aligned (the PAGES segment at $0C00, game.cfg), so the
; 8-bit indexes wrap for free.

        .export _rx_ring

        .segment "PAGES"
_rx_ring: .res 256
