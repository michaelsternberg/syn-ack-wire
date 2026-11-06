/* Free memory in the guest, after the broker is loaded: for the sizes table in README.md */
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
int main(void)
{
    printf("chip free %lu largest %lu, fast free %lu\n", (unsigned long)AvailMem(MEMF_CHIP),
           (unsigned long)AvailMem(MEMF_CHIP | MEMF_LARGEST), (unsigned long)AvailMem(MEMF_FAST));
    fflush(stdout);
    return 0;
}
