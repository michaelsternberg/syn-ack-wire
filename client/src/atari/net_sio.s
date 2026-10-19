; net_sio.s -- the platform.h network calls for the Atari: FujiNet N1: over raw SIO.
; No N: CIO handler and no fujinet-lib. Every call returns A = 0 when fine,
; else the SIO status (128-255) or NET_CLOSED; X = 0.
;
; PROCEED: FujiNet pulls SIO pin 9 low when the N: device has data or news. It
; reaches the PIA's CA1 input. We can't just read the latched flag in PACTL,
; because the OS vertical blank reads PORTA for the joysticks every frame, and
; that clears it. So the CA1 interrupt is enabled and a 6-byte handler sets
; `ready`. net_poll only spends an SIO status call when `ready` is set, or once
; a second as a safety net for a missed pulse or a quiet disconnect.

        .export _net_open, _net_poll, _net_send, _net_close
        .export _rx_head, _rx_tail, _tx
        .import _rx_ring

DDEVIC  = $0300
DUNIT   = $0301
DCOMND  = $0302
DSTATS  = $0303
DBUFLO  = $0304
DBUFHI  = $0305
DTIMLO  = $0306
DBYTLO  = $0308
DBYTHI  = $0309
DAUX1   = $030A
DAUX2   = $030B
SIOV    = $E459
SOUNDR  = $41           ; 0 = no beeps during SIO
RTCLOK  = $14           ; jiffy counter, low byte
VPRCED  = $0202         ; PROCEED interrupt vector
PORTA   = $D300         ; reading it clears the CA1 interrupt flag
PACTL   = $D302         ; bit 0 enables the CA1 (PROCEED) interrupt; bit 3 is the
                        ; cassette motor, so only ever OR into it
SAFETY  = 60            ; jiffies between status calls with no PROCEED (about 1 s)

NDEV    = $71           ; N1:
TIMEOUT = 15            ; DTIMLO units of 64 frames, about a second each
NET_CLOSED = $FF

; command index -> SIO command, DSTATS direction
OPEN    = 0
STATUS  = 1
READ    = 2
WRITE   = 3
CLOSE   = 4

        .bss
_rx_head: .res 1        ; next byte the frame assembler takes
_rx_tail: .res 1        ; next byte SIO fills
_tx:    .res 16
stat:   .res 4          ; bytes waiting lo/hi, connected, error
n:      .res 1
ready:  .res 1          ; non-zero: FujiNet has something, or bytes were left behind
last:   .res 1          ; RTCLOK at the last status call

        .rodata
cmd:    .byte 'O', 'S', 'R', 'W', 'C'
dir:    .byte $80, $40, $40, $80, $00

        .code

; unsigned char net_open(const char *url)
; The devicespec is sent as a 256-byte block; bytes after the NUL are ignored.
_net_open:
        sta DBUFLO
        stx DBUFHI
        ldy #<proceed
        sty VPRCED
        ldy #>proceed
        sty VPRCED+1
        lda PORTA               ; drop any stale PROCEED edge
        lda PACTL
        ora #1
        sta PACTL               ; CA1 interrupt on
        ldy #0
        sty SOUNDR              ; no SIO beeps: we poll many times a second
        sty _rx_head            ; anything left from an old connection is junk
        sty _rx_tail
        sty DBYTLO
        sty DAUX2               ; translation 0: bytes pass untouched
        iny
        sty DBYTHI              ; 256 bytes
        sty ready               ; Y = 1: check status on the first poll
        lda #12
        sta DAUX1               ; read + write
        ldx #OPEN
        jmp sio

; PROCEED handler. The OS IRQ code pushed A before jumping here.
proceed:
        lda PORTA               ; clear the PIA's interrupt flag
        lda #1
        sta ready
        pla
        rti

; unsigned char net_poll(void)
; Cheap unless PROCEED fired (or a second has passed): then one status call and,
; if bytes are waiting, one read of as many as fit before the ring's page end
; without overrunning unparsed bytes. Bytes left behind keep `ready` set.
_net_poll:
        lda ready
        bne @go
        lda RTCLOK
        sec
        sbc last
        cmp #SAFETY
        bcc ok
@go:    lda #0
        sta ready               ; a PROCEED from here on means "poll again"
        lda RTCLOK
        sta last
        lda #4
        ldx #<stat
        ldy #STATUS
        jsr xfer_hi
        bne done
        lda stat+1
        bne @cap                ; 256 or more waiting: take 255 now
        lda stat
        bne @have
        lda stat+2              ; nothing waiting: still connected?
        bne ok
        lda #NET_CLOSED
        rts                     ; X = 0 from sio
@cap:   lda #$FF
@have:  sta n
        lda _rx_head            ; free = head - tail - 1
        clc
        sbc _rx_tail
        cmp n
        bcs :+
        sta n
:       lda _rx_tail            ; room to page end = 256 - tail (tail 0: whole page)
        beq :+
        lda #0
        sec
        sbc _rx_tail
        cmp n
        bcs :+
        sta n
:       lda n
        beq @more               ; ring full: the assembler drains it first
        ldx _rx_tail
        ldy #READ
        jsr xfer_ring
        bne done
        lda _rx_tail
        clc
        adc n
        sta _rx_tail
        lda stat+1              ; all of it read?
        bne @more
        lda stat
        cmp n
        beq ok
@more:  inc ready               ; no: read the rest on the next poll
ok:     lda #0
        tax
done:   rts

; void net_close(void)
_net_close:
        ldx #CLOSE
        bne sio                 ; always

; unsigned char net_send(unsigned char len) -- sends tx[0..len-1]
_net_send:
        ldx #<_tx
        ldy #WRITE
        bne xfer                ; always

; A = length (1-255), X = buffer low byte, Y = command. xfer: buffer in tx's page,
; xfer_hi: in stat's page, xfer_ring: in the ring's page. Length doubles as AUX1/2.
xfer_ring:
        pha
        lda #>_rx_ring
        bne xfer_page           ; always
xfer_hi:
        pha
        lda #>stat
        bne xfer_page           ; always
xfer:   pha
        lda #>_tx
xfer_page:
        sta DBUFHI
        stx DBUFLO
        pla
        sta DBYTLO
        cpy #STATUS
        bne :+
        lda #0                  ; status takes no AUX length
:       sta DAUX1
        lda #0
        sta DBYTHI
        sta DAUX2
        tya
        tax

; X = command index. Returns A = 0 if SIO said OK, else its status; X = 0.
sio:    lda #NDEV
        sta DDEVIC
        lda #1
        sta DUNIT
        lda cmd,x
        sta DCOMND
        lda dir,x
        sta DSTATS
        lda #TIMEOUT
        sta DTIMLO
        jsr SIOV
        ldx #0
        tya                     ; Y = SIO status, 1 = OK
        cmp #1
        bne :+
        txa
:       rts
