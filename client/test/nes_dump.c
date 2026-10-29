/* autoplay's dump hook in the NES's test build (make nes-auto): it counts the
 * dump in dump_seq and waits, at most 3 frames, for the emulator to say it has
 * saved the screen (tools/nes.lua writes dump_ack), as atari800's monitor does
 * on the Atari's BRK. The screen doesn't change meanwhile. */
#include "../src/nes/text.h"

unsigned char dump_seq, dump_ack;

void pc_dump(void)
{
    unsigned char t = frames;

    ++dump_seq;
    while (dump_ack != dump_seq && (unsigned char)(frames - t) < 3)
        ;
}
