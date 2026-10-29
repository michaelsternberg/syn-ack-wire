; vdp.asm -- writes to the TMS9918A's memory while the screen is drawn, each
; 29 cycles at least after the last (8 us: in Graphics II the VDP may take that
; long to find a slot). Arguments are in variables (vdp.h); with the address
; set, the first byte follows 30 cycles later. The caller holds VDP_LOCK.

        SECTION code_user

        PUBLIC  _vdp_put8, _vdp_fill8, _vdp_names
        PUBLIC  _vdp_adr, _vdp_src, _vdp_val, _vdp_n

setadr:
        ld      hl,(_vdp_adr)
        ld      a,l
        out     ($BF),a
        ld      a,h
        or      $40
        out     ($BF),a
        ret

; 8 bytes from vdp_src: 37 cycles a byte
_vdp_put8:
        call    setadr
        ld      hl,(_vdp_src)
        ld      b,8
put8:   ld      a,(hl)
        out     ($BE),a
        inc     hl
        djnz    put8
        ret

; vdp_val, 8 times: 37 cycles a byte
_vdp_fill8:
        call    setadr
        ld      a,(_vdp_val)
        ld      b,8
fill8:  out     ($BE),a
        nop
        nop
        nop
        djnz    fill8
        ret

; vdp_n bytes (0 = 256), vdp_val, vdp_val + 1...: a stretch of the name table, 37 cycles a byte
_vdp_names:
        call    setadr
        ld      a,(_vdp_n)
        ld      b,a
        ld      a,(_vdp_val)
names:  out     ($BE),a
        inc     a
        nop
        nop
        djnz    names
        ret

        SECTION bss_user
_vdp_adr:       defs    2
_vdp_src:       defs    2
_vdp_val:       defs    1
_vdp_n:         defs    1
