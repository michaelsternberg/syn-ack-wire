/* The Adam's TMS9918A: ports BEh (data) and BFh (control). Its interrupt is
 * the Z80's NMI (nmi.asm), which can't be masked, and reading the VDP's
 * status in it would reset the address latch under an address being set or a
 * burst of data: so VDP_LOCK/VDP_UNLOCK tell it to wait (as
 * fujinet-multiplayer-mule's src/tms9918/vdp.h). While the screen is drawn,
 * accesses must be 8 us (29 cycles) apart: vdp.asm's loops keep to that. */
#ifndef VDP_H
#define VDP_H

__sfr __at 0xBE VDP_DATA;
__sfr __at 0xBF VDP_CTRL;

extern volatile unsigned char vdp_busy, vdp_pend;
void vdp_catch_up(void);                /* nmi.asm: the frame the NMI left */
#define VDP_LOCK()   (vdp_busy = 1)
#define VDP_UNLOCK() do { vdp_busy = 0; if (vdp_pend) vdp_catch_up(); } while (0)

#define VDP_WADDR(a) (VDP_CTRL = (unsigned char)(a), VDP_CTRL = (unsigned char)((a) >> 8) | 0x40)
#define VDP_REG(r, v) (VDP_CTRL = (unsigned char)(v), VDP_CTRL = 0x80 | (r))

/* VRAM: Graphics II with the name table 0-255 in each third, so a cell's
 * pattern and colours are at row * 256 + column * 8 */
#define VR_PAT      0x0000
#define VR_NAME     0x1800
#define VR_SAT      0x1B00
#define VR_COL      0x2000
#define VR_SPAT     0x3800

/* vdp.asm: their arguments are these variables */
extern unsigned vdp_adr;
extern const unsigned char *vdp_src;
extern unsigned char vdp_val, vdp_n;
void vdp_put8(void);                    /* 8 bytes from vdp_src at vdp_adr */
void vdp_fill8(void);                   /* vdp_val 8 times at vdp_adr */
void vdp_names(void);                   /* vdp_n bytes counting up from vdp_val at vdp_adr */

#endif
