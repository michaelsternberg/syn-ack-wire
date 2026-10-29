; crt0.s -- the NES client's iNES header, start-up, NMI and vectors.
;
; The NMI does a frame's work, in this order:
;   - in the vertical blank: the sprites' DMA, then ppu_nmi's writes (the
;     attribute rows, the title's static, the rows queued for this frame), then
;     the scroll, the name table to show (the game's, or the instructions')
;     and the display's on/off;
;   - after it, where time no longer matters: the pad (pad.s), the chords'
;     envelope (sound.s), and ppu_after, which queues the next frame's rows.
; While ppu_direct is set the program owns the PPU (the display is off: it
; writes the tiles and name tables itself), and the NMI leaves the PPU alone.
;
; Nothing here, in ppu.s, pad.s or sound.s ever writes $5500-$57FF: a FujiNet
; transaction in flight survives any number of NMIs as long as that holds
; (fujinet-lib's fujinet-bus-nes.h). fujinet-lib waits for the cartridge in a
; counted loop with interrupts on, so the frame counter keeps time through it.
;
; At start-up the CPU cycles between two NMIs tell an NTSC NES (29781 a frame,
; 60 a second) from a PAL one or a Dendy (33248 and 35464, 50 a second):
; ticks_per_sec() (input.c) gives what it found.
        .export         __STARTUP__ : absolute = 1
        .export         _exit
        .exportzp       _frames, _ppu_direct
        .export         _ppu_ctrl, _ppu_mask, _nes_tps
        .import         initlib, donelib, callmain, zerobss, copydata
        .import         ppu_nmi, ppu_after, pad_nmi, snd_nmi
        .importzp       c_sp
        .include        "zeropage.inc"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
OAMDMA    = $4014

CTRL      = $88                 ; NMI on, background tiles at $0000, sprites (8x8) at $1000
TV_PAL    = 2840                ; passes of tv's loop in a frame: NTSC ~2680, PAL ~2995, Dendy ~3200

; ---------------------------------------------------------------------------
; iNES 1.0: mapper 0, 32K PRG, no CHR-ROM (8K CHR-RAM), vertical mirroring
; (two name tables side by side: the game's at $2000, the instructions' at
; $2400). The battery bit asks for the WRAM at $6000, as fujinet-config's and
; the lobby's NES images do; the cartridge serves it to a mapper-0 image.
.segment "HEADER"
        .byte   "NES", $1A
        .byte   2               ; PRG: 2 x 16K
        .byte   0               ; CHR: RAM
        .byte   $03             ; mapper 0, vertical mirroring, WRAM
        .byte   0
        .byte   0, 0, 0, 0, 0, 0, 0, 0

; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
_frames:     .res 1             ; the frame counter: ticks()
_ppu_direct: .res 1             ; the program has the PPU: the NMI keeps off it

.segment "OAM"
oam:         .res 256           ; the sprites, DMA'd every frame (text.h: OAM)

.segment "LORAM"
_ppu_ctrl:   .res 1             ; PPUCTRL for the next frame: bit 0 picks the name table shown
_ppu_mask:   .res 1             ; PPUMASK for the next frame: 0 = the display off
_nes_tps:    .res 1             ; frames a second: 60, or 50

; ---------------------------------------------------------------------------
.segment "STARTUP"

start:
        sei
        cld
        ldx     #$FF
        txs
        inx
        stx     PPUCTRL         ; no NMI yet
        stx     PPUMASK         ; the display off
        stx     $4010           ; no DMC interrupt
        stx     $4015           ; the APU silent (snd_init turns it on)
        lda     #$40
        sta     $4017           ; no APU frame interrupt
        bit     PPUSTATUS
@v1:    bit     PPUSTATUS       ; the PPU needs two frames to warm up
        bpl     @v1

        txa                     ; all of RAM zero, the stack's page too: nothing is on it yet
@clr:   sta     $00,x
        sta     $0100,x
        sta     $0300,x
        sta     $0400,x
        sta     $0500,x
        sta     $0600,x
        sta     $0700,x
        inx
        bne     @clr
        lda     #$F8            ; every sprite below the screen
@ofs:   sta     oam,x
        inx
        bne     @ofs

@v2:    bit     PPUSTATUS
        bpl     @v2

        jsr     zerobss
        jsr     copydata
        lda     #<$0800         ; the C stack: $0500-$07FF, down from the top
        ldx     #>$0800
        sta     c_sp
        stx     c_sp+1
        lda     #1
        sta     _ppu_direct     ; the display stays off until the first screen is drawn
        lda     #CTRL
        sta     _ppu_ctrl
        sta     PPUCTRL         ; the frame clock
        jsr     tv
        jsr     initlib
        jsr     callmain
_exit:  jsr     donelib
        jmp     start

; Count a frame's passes of an 11-cycle loop, from one NMI to the next.
tv:     lda     _frames
@sync:  cmp     _frames
        beq     @sync
        lda     _frames
        ldx     #0
        ldy     #0
@pass:  inx
        bne     @same
        iny
@same:  cmp     _frames
        beq     @pass
        lda     #60
        cpy     #>TV_PAL
        bcc     @tps
        bne     @pal
        cpx     #<TV_PAL
        bcc     @tps
@pal:   lda     #50
@tps:   sta     _nes_tps
        rts

; ---------------------------------------------------------------------------
.segment "CODE"

nmi:    pha
        txa
        pha
        tya
        pha
        inc     _frames
        lda     _ppu_direct
        bne     @late
        lda     #0
        sta     OAMADDR
        lda     #>oam
        sta     OAMDMA
        jsr     ppu_nmi
        lda     _ppu_ctrl
        sta     PPUCTRL
        lda     #0
        sta     PPUSCROLL
        sta     PPUSCROLL
        lda     _ppu_mask
        sta     PPUMASK
@late:  jsr     pad_nmi
        jsr     snd_nmi
        lda     _ppu_direct
        bne     @out
        jsr     ppu_after
@out:   pla
        tay
        pla
        tax
        pla
irq:    rti

; ---------------------------------------------------------------------------
.segment "VECTORS"
        .word   nmi
        .word   start
        .word   irq

.segment "CLAIM"
        .byte   "FUJI"
