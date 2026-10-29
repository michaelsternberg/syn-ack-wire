; eoscall.asm -- the keyboard through EOS, straight into its jump table, as
; fujinet-multiplayer-mule's src/adam/eoscall.asm (z88dk's eos.lib is built with
; sccz80, whose arguments come in the other order from zsdcc's). IX and IY,
; which EOS may change, are kept.
;
; void eos_kbd_start(void)      ask the keyboard for a character
; unsigned char eos_kbd_end(void)   0 none yet, 1 failed (ask again), else the character

        SECTION code_user

        PUBLIC  _eos_kbd_start, _eos_kbd_end

        defc    START_RD_KBD = $FCA8
        defc    END_RD_KBD   = $FC4B    ; C finished (then Z: A the character)

_eos_kbd_start:
        push    ix
        push    iy
        call    START_RD_KBD
        pop     iy
        pop     ix
        ret

_eos_kbd_end:
        push    ix
        push    iy
        call    END_RD_KBD
        ld      l,0
        jr      nc,kb_out       ; not yet
        ld      l,a
        jr      z,kb_out        ; the character
        ld      l,1
kb_out:
        pop     iy
        pop     ix
        ret
