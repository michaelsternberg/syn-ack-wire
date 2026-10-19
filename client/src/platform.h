/* The contract between common code and each platform (src/<platform>/).
 * Keep it tiny: every function here is written once per machine. */
#ifndef PLATFORM_H
#define PLATFORM_H

/* ------------------------------------------------------------------ network
 * One TCP connection to the server. Received bytes land in rx_ring, a 256-byte
 * ring: net_poll() advances rx_tail, the frame assembler (proto.c) advances
 * rx_head. Both are bytes, so they wrap for free. The ring never holds more
 * than 255 bytes, so head == tail always means empty.
 *
 * Return codes: 0 = fine. Anything else means the link is down: the platform's
 * own error code (Atari: SIO status 128-255), or NET_CLOSED. */
#define NET_CLOSED 0xFF

extern unsigned char rx_ring[256];
extern unsigned char rx_head, rx_tail;
extern unsigned char tx[16];            /* the next client frame, built in place */

unsigned char __fastcall__ net_open(const char *url);   /* "N:TCP://host:port/" */
unsigned char net_poll(void);           /* move waiting bytes into rx_ring */
unsigned char __fastcall__ net_send(unsigned char len); /* send tx[0..len-1] */
void net_close(void);

/* ------------------------------------------------------------------ input, time */
#define K_RETURN '\r'
#define K_DELETE '\b'
#define K_OPTION 0x01                   /* toggles move hints (Atari: the OPTION key) */
#define TICKS_PER_SEC 60                /* PAL machines tick at 50: clocks run 20% slow */

unsigned char in_key(void);             /* next key as uppercase ASCII, or 0 if none */
unsigned char ticks(void);              /* free-running 8-bit frame counter */

#endif
