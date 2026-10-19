; Just enough CIO to read one file a byte at a time on IOCB #1. D: needs DOS,
; so with no DOS loaded cio_open fails with 130 (no such device).
;
; Each call returns the CIO status: 1 = OK, 3 = OK and this is the last byte,
; 128-255 = error (136 = end of file); X = 0.

        .export _cio_open, _cio_get, _cio_close, _cio_byte

CIOV    = $E456
IOCB    = $0350             ; IOCB #1
ICCOM   = IOCB + 2
ICBAL   = IOCB + 4
ICBAH   = IOCB + 5
ICBLL   = IOCB + 8
ICBLH   = IOCB + 9
ICAX1   = IOCB + 10
ICAX2   = IOCB + 11

        .bss
_cio_byte:  .res 1          ; the byte cio_get read

        .code

; unsigned char __fastcall__ cio_open(const char *name)   "D:NAME.EXT", EOL-terminated
_cio_open:
        sta ICBAL
        stx ICBAH
        lda #4              ; read
        sta ICAX1
        lda #0
        sta ICAX2
        lda #3              ; OPEN
        bne call

; unsigned char cio_get(void)
_cio_get:
        lda #0              ; buffer length 0: one byte, returned in A
        sta ICBLL
        sta ICBLH
        lda #7              ; GET CHARACTERS
        sta ICCOM
        ldx #$10
        jsr CIOV
        sta _cio_byte
        jmp ret

; unsigned char cio_close(void)
_cio_close:
        lda #12             ; CLOSE
call:   sta ICCOM
        ldx #$10
        jsr CIOV
ret:    tya
        ldx #0
        rts
