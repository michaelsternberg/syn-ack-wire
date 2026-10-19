/* Size bake-off: the same calls linked against net_sio.s or fujinet-lib. */
#include "../src/platform.h"
#include "../src/common/proto.h"

void main(void)
{
    if (net_open("N:TCP://localhost:6502/"))
        return;
    tx[0] = 'R';
    net_send(1);
    while (!net_poll())
        if (proto_next() == 'e')
            break;
    net_close();
}
