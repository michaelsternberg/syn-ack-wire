/* A flaky serial line, for testing the MS-DOS network code (src/msdos/net_fn.c)
 * against the short failures a real RS-232 FujiNet has: build with
 * -Dnetwork_status=glitch_status and this file, and every GLITCH_EVERY BIOS
 * ticks the status call fails for GLITCH_FOR ticks, as an exchange spoiled on
 * the line would. The default, 1 second in 20, should never cost the
 * connection; the server's log shows any redial. */
#undef network_status
#include <fujinet-network.h>

#ifndef GLITCH_EVERY
#define GLITCH_EVERY 364                /* 20 s of BIOS ticks */
#endif
#ifndef GLITCH_FOR
#define GLITCH_FOR 18                   /* 1 s */
#endif

#define BIOS_TICKS (*(volatile unsigned long __far *)0x0040006CL)

FN_ERR glitch_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err)
{
    if (BIOS_TICKS % GLITCH_EVERY < GLITCH_FOR)
        return FN_ERR_IO_ERROR;
    return network_status(devicespec, bw, c, err);
}
