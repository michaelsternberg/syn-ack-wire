/* What the H89 files share: the clock and the console, under Heath/Zenith CP/M 2.2.04. */
#ifndef H89_H
#define H89_H


/* The BIOS counts the H89's 2 ms clock interrupt in this word (fujinet-lib's bus/h89 waits on it too).
 * TICK is it in eighths, about 62 a second: the 8-bit frame counter the common code wants. */
#define JIFFIES (*(volatile unsigned short *)0x000B)
#define TICK    ((unsigned char)(JIFFIES >> 3))
#define TICKS_PER_SEC 62

/* CP/M's BDOS, as z88dk's <cpm.h> declares it: that header can't be included with proto_tab.h (both define
 * F_FULL). BDOS 6 is the console as it is: with 0xFF, a key or 0; with a byte, that byte out. In the PC
 * build for the manual's captures (PC_SCREEN) there is no CP/M: every call answers 0xFF, "no file". */
#ifdef PC_SCREEN
static inline int bdos(int func, int arg) { (void)func; (void)arg; return 0xFF; }
#else
#include <sys/compiler.h>               /* __LIB__, __smallc, __z88dk_callee */
extern int __LIB__ bdos_callee(int func, int arg) __smallc __z88dk_callee;
#define bdos(a, b) bdos_callee(a, b)
#endif

#endif
