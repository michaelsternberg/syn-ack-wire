; The receive ring lives in page 6: free for programs, and page-aligned, so an
; 8-bit index wraps for free and costs no RAM in the program image.
        .export _rx_ring
_rx_ring = $0600
