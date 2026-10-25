/* The autoplay build's screen dumps on MS-DOS (make msdos-test): test/autoplay.c
 * calls pc_dump() at each new prompt and every DUMP_TICKS. Each dump is the 40x24
 * game area as character and attribute bytes, 1920 bytes, written to SCRnnn.BIN in
 * the current directory. tools/screen2txt.py --msdos shows them as text. */
#include <fcntl.h>
#include <io.h>
#include <string.h>

void pc_dump(void)
{
    static unsigned n;
    static char name[] = "SCR000.BIN";
    static unsigned short rows[24 * 40];
    unsigned short __far *v = *(volatile unsigned char __far *)0x00400049L == 7
        ? (unsigned short __far *)0xB0000000L : (unsigned short __far *)0xB8000000L;
    unsigned char r;
    int fd;
    if (n > 999)
        return;
    for (r = 0; r < 24; ++r)
        _fmemcpy(rows + r * 40, v + r * 80 + 20, 40 * 2);
    name[3] = '0' + n / 100;
    name[4] = '0' + n / 10 % 10;
    name[5] = '0' + n % 10;
    ++n;
    fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0)
        return;
    write(fd, rows, sizeof rows);
    close(fd);
}
