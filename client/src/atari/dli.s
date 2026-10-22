; dli.s -- the playfield's color bands. Display list interrupts change the
; background (COLPF2) between rows:
;   end of the blank lines above row 0    purple behind the title bar: only
;                                         while the link is down (NO SIGNAL)
;   end of row 0 (the title bar)          back to COLOR2: only with the above
;   end of row 12 (the NETRUNNER header)  green one step up behind the seats
;   end of row 18 (the last seat)         back to COLOR2 for the log
;   end of row 21 (the last log line)     purple behind the info row and the
;                                         question: only while this seat has a
;                                         decision to make
;   end of row 23 (the last row)          back to COLOR2, out of sight
; The last one matters: the OS copies COLOR2 to COLPF2 in its vertical blank's
; second stage, which it skips when the blank interrupts an IRQ (a key, SIO),
; and the purple would then run from the top of the screen. band() sets or
; clears the DLI bits in the OS display list.
; A DLI is an NMI: it may interrupt SIO's serial IRQs, and at standard speed
; (about 930 cycles a byte) its few dozen cycles leave plenty of margin.
;
; void dli(void)                            the handler, for VDSLST
; void __fastcall__ band(unsigned char m)   the bands: 0 off, 1 on (the
;                                           playfield); + 2 the title alert,
;                                           + 4 the purple at the bottom

        .export _dli, _band
        .importzp ptr1

COLPF2  = $D018
WSYNC   = $D40A
VCOUNT  = $D40B         ; scan line / 2: row r starts at 16 + 4r
COLOR2  = $02C6
SDLSTL  = $0230         ; the OS display list

SEATS   = $C2           ; display_text.c's PM_COLOR: the Grid panel's green
INFO    = $60           ; purple, green's complement
ROW_SEATS = 13          ; display_text.c: the seats' rows
ROW_LOG = 19            ;   the log's
ROW_INFO = 22           ;   the info row, then the question

        .data
next:   .byte SEATS             ; the color the coming DLI sets

        .bss
mode:   .res 1                  ; band()'s; bit 1 = the title alert, bit 2 = the
                                ; bottom's purple

        .code
; The new color must be in COLPF2 before the end of the DLI's own line, and a
; text line leaves the CPU few cycles (ANTIC fetches the font). So the color is
; worked out ahead: this DLI stores `next` at once, then, with time to spare,
; picks the next one's from where the beam is now. Just past the WSYNC, VCOUNT
; reads row r's last line or the line after (16 + 4r + 3 or 4; the blank lines
; above row 0 count as row -1), so each test sits between that and the next
; DLI's row. A band switched on mid-frame is put right by the next DLI.
AFTER   = 16 + 6                ; + 4r: past row r's DLI, short of any later one

_dli:   pha
        lda next
        sta WSYNC               ; change it between lines
        sta COLPF2
        lda VCOUNT
        cmp #AFTER - 4
        bcc @back               ; after the top's: row 0's puts it back
        cmp #AFTER
        bcc @seats              ; after row 0's: row 12's
        cmp #AFTER + 4 * (ROW_SEATS - 1)
        bcc @back               ; after row 12: row 18's puts it back
        cmp #AFTER + 4 * (ROW_LOG - 1)
        bcc @ask                ; after row 18: row 21's may make it purple
        cmp #AFTER + 4 * (ROW_INFO - 1)
        bcc @back               ; after row 21: row 23's puts it back
        lda mode                ; after row 23: next frame's top, if alerting,
        and #2
        bne @info
@seats: lda #SEATS              ;   else its row 12
        bne @set
@ask:   lda mode
        and #4
        beq @back
@info:  lda #INFO
        bne @set
@back:  lda COLOR2
@set:   sta next
        pla
        rti

; A text row's instruction is $02, + $80 for a DLI on its last line. The
; display list starts with three 8-line blanks ($70), the last one at 2; row 0
; is the 3-byte LMS instruction ($42) at 3, so row r is at 5 + r.
_band:  sta mode
        ldy SDLSTL
        sty ptr1
        ldy SDLSTL+1
        sty ptr1+1
        lsr a                   ; carry = the bands
        lda #$04
        ror a                   ; $02, or $82 with the carry
        ldy #5 + ROW_SEATS - 1
        sta (ptr1),y
        ldy #5 + ROW_LOG - 1
        sta (ptr1),y
        ldy #5 + ROW_INFO - 1
        sta (ptr1),y
        ldy #5 + 23
        sta (ptr1),y
        lda mode
        lsr a
        lsr a                   ; carry = the alert
        php
        lda #$E0
        ror a                   ; $70, or $F0
        ldy #2
        sta (ptr1),y
        plp
        lda #$84
        ror a                   ; $42, or $C2
        iny
        sta (ptr1),y
        rts
