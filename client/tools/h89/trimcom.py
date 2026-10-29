#!/usr/bin/env python3
"""trimcom.py MAP COM: cut a z88dk CP/M .COM short where its uninitialised variables start.

z88dk's +cpm writes the zeros of the uninitialised variables (BSS) into the .COM, which the startup clears
anyway (CRT_INITIALIZE_BSS=1): for SYNACK.COM 15 KB more on the disk, and for the H17 to read at every
start. MAP is zcc's -m map. Everything from __BSS_head on must be BSS and zero, or it says so and leaves
the file. After FujiNet's M.U.L.E. client (fujinet-multiplayer-mule, clients/mekkogx/tools/h89/trimcom.py)."""
import re
import sys

syms = {}
for line in open(sys.argv[1]):
    m = re.match(r"(\S+)\s+= \$([0-9A-F]+) ;", line)
    if m:
        syms.setdefault(m.group(1), int(m.group(2), 16))
head, end, tail = syms["__BSS_head"], syms["__BSS_END_tail"], syms["__tail"]
if end != tail:
    sys.exit("trimcom.py: something follows the BSS: left as it is")
data = open(sys.argv[2], "rb").read()
keep = head - 0x100                     # a .COM loads at 0100h
if any(data[keep:]):
    sys.exit("trimcom.py: the BSS in the file is not all zero: left as it is")
open(sys.argv[2], "wb").write(data[:keep])
print("trimcom.py: %s, %d bytes (%d of BSS cut); it runs up to %04Xh" % (sys.argv[2], keep, len(data) - keep, tail))
