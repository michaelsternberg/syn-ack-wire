; nmi.asm -- the Adam's frame: the VDP's interrupt is the Z80's NMI (the CRT
; jumps here from 0066h: CRT_ENABLE_NMI=2 in the Makefile). Each frame it counts
; `frames` (ticks()), plays the sound (sound.asm), reads controller 1 and, on
; the title, writes the static into its tiles.
;
; Reading the VDP's status acknowledges the interrupt, but also resets its
; address latch, which would spoil an address being set or a burst of data
; under way. While the program is at the VDP (vdp_busy, vdp.h) the status is
; left and vdp_pend set; the VDP holds its interrupt line down meanwhile, so no
; other NMI comes, and VDP_UNLOCK catches up (vdp_catch_up), all but the static.
; As fujinet-multiplayer-mule's src/adam/nmi.asm.
;
; Controller 1 is read straight from its port (EOS's poller doesn't run: this
; NMI is not EOS's), as the NES's pad (src/nes/pad.s): what was newly pressed
; goes into an 8-event ring that input.c empties (the NMI writes pad_head, the
; program pad_tail). An event is the buttons newly pressed in a frame, as the
; NES's: left fire = A $80, right fire = B $40, keypad * = SELECT $20, # =
; START $10, up $08, down $04, left $02, right $01. SELECT is a shift: a press
; of another button while it is held comes with $20 set, and SELECT alone
; makes its own event, $20, when it is let go without one. A keypad digit
; newly pressed goes to pad_digit ('0'-'9'), for input.c to take.

        SECTION code_user

        PUBLIC  _z80_nmi, _vdp_catch_up
        PUBLIC  _vdp_busy, _vdp_pend, _frames
        PUBLIC  _pad_ring, _pad_head, _pad_tail, _pad_now, _pad_digit
        PUBLIC  _noise, _noise_on, _st_adr, _st_n
        EXTERN  snd_frame

        defc    SEL = $20

_z80_nmi:
        push    af
        ld      a,(_vdp_busy)
        or      a
        jr      nz,busy
        in      a,($BF)         ; the status: acknowledges it
        push    bc
        push    de
        push    hl
        call    frame
        ld      a,(_noise_on)
        or      a
        call    nz,static
        pop     hl
        pop     de
        pop     bc
        pop     af
        retn
busy:
        ld      a,1
        ld      (_vdp_pend),a
        pop     af
        retn

; void vdp_catch_up(void): the NMI came while the VDP was busy; take it now.
; The NMI keeps out meanwhile, and one that came is taken too.
_vdp_catch_up:
        ld      a,1
        ld      (_vdp_busy),a
        xor     a
        ld      (_vdp_pend),a
        in      a,($BF)         ; (the NMI may have beaten us to it)
        rla
        call    c,frame
        xor     a
        ld      (_vdp_busy),a
        ld      a,(_vdp_pend)
        or      a
        jr      nz,_vdp_catch_up
        ret

frame:
        ld      hl,_frames
        inc     (hl)
        call    snd_frame
        ; fall through

; ---------------------------------------------------------------- controller 1
pad:
        ld      a,(_pad_now)
        ld      (prev),a
        out     ($C0),a         ; the stick and left fire
        in      a,($FC)
        cpl                     ; 1 north, 2 east, 4 south, 8 west, $40 left fire
        ld      c,a
        xor     a
        bit     0,c
        jr      z,p_s
        or      $08
p_s:    bit     2,c
        jr      z,p_w
        or      $04
p_w:    bit     3,c
        jr      z,p_e
        or      $02
p_e:    bit     1,c
        jr      z,p_f
        or      $01
p_f:    bit     6,c
        jr      z,p_k
        or      $80
p_k:    ld      b,a
        out     ($80),a         ; the keypad and right fire
        in      a,($FC)
        bit     6,a             ; (active low)
        jr      nz,p_kp
        set     6,b
p_kp:   and     $0F
        ld      e,a
        ld      d,0
        ld      hl,keypad
        add     hl,de
        ld      a,(hl)          ; '0'-'9', '*', '#', or 0
        ld      hl,kp_prev
        cp      (hl)
        ld      (hl),a
        jr      z,p_held        ; held: nothing new
        cp      '0'
        jr      c,p_held
        ld      (_pad_digit),a  ; a digit, newly pressed
p_held: cp      '*'
        jr      nz,p_hash
        set     5,b
p_hash: cp      '#'
        jr      nz,p_ev
        set     4,b

p_ev:   ld      a,b
        ld      (_pad_now),a
        ld      a,(prev)
        cpl
        and     b
        and     ~SEL & $FF
        ld      (new),a
        ld      a,b
        and     SEL
        jr      z,p_up
        ld      a,(prev)        ; SELECT held: a new press of it starts a shift
        and     SEL
        jr      nz,p_shift
        ld      (chord),a       ; (a = 0)
p_shift:
        ld      a,(new)
        or      a
        ret     z
        or      SEL
        ld      (chord),a
        jr      push
p_up:   ld      a,(prev)        ; SELECT not held: let go just now, alone?
        and     SEL
        jr      z,p_plain
        ld      a,(chord)
        or      a
        jr      nz,p_plain
        ld      a,SEL
        call    push
p_plain:
        ld      a,(new)
        or      a
        ret     z
push:   ld      c,a             ; the event
        ld      a,(_pad_head)
        ld      e,a
        ld      d,0
        ld      hl,_pad_ring
        add     hl,de
        ld      (hl),c
        inc     a
        and     7
        ld      hl,_pad_tail
        cp      (hl)
        ret     z               ; full: dropped
        ld      (_pad_head),a
        ret

; the keypad's codes (its 4 bits, active low) to keys
keypad: defb    0, '8', '4', '5', 0, '7', '#', '2', 0, '*', '0', '9', '3', '1', '6', 0

; ---------------------------------------------------------------- the title's static
; 8 random bytes into each of the st_n tiles at st_adr (text.c txt_title),
; from a different place in `noise` each frame (an 8-bit LFSR). In the
; vertical blank: no wait between bytes.
static:
        ld      a,(noise_at)    ; never 0, so it starts at 1
        or      a
        jr      nz,st_lf
        inc     a
st_lf:  add     a,a
        jr      nc,st_at
        xor     $1D
st_at:  ld      (noise_at),a
        ld      c,a
        ld      a,(_st_n)
        or      a
        ret     z
        ld      b,a
        ld      hl,_st_adr
st_tile:
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,e
        out     ($BF),a
        ld      a,d
        or      $40
        out     ($BF),a
        push    hl
        push    bc
        ld      b,0
        ld      hl,_noise
        add     hl,bc           ; noise + c: up to 7 past its 256 (it has 264)
        ld      b,8
st_byte:
        ld      a,(hl)
        out     ($BE),a
        inc     hl
        djnz    st_byte
        pop     bc
        pop     hl
        ld      a,c
        add     a,8
        ld      c,a
        djnz    st_tile
        ret

        SECTION bss_user
_vdp_busy:      defs    1
_vdp_pend:      defs    1
_frames:        defs    1
_pad_ring:      defs    8
_pad_head:      defs    1
_pad_tail:      defs    1
_pad_now:       defs    1       ; the buttons held, as the last frame found them
_pad_digit:     defs    1
_noise:         defs    264     ; random bytes (text.c fills it)
_noise_on:      defs    1       ; the title's static runs
_st_adr:        defs    16      ; the static's tiles: their patterns' addresses
_st_n:          defs    1
prev:           defs    1
new:            defs    1
chord:          defs    1       ; a button went with this SELECT
kp_prev:        defs    1
noise_at:       defs    1
