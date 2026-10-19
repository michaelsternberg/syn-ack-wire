#!/usr/bin/env python3
"""Generate the client's frame-length table from server/acquire/proto.py, so the two can't drift.

Usage:
    gen_proto.py h      print C header (client/src/common/proto_tab.h)
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "server"))
from acquire import proto  # noqa: E402

VAR, BAD = 0xFE, 0xFF


def gen_h():
    L = ["/* GENERATED from server/acquire/proto.py by tools/gen_proto.py -- do not edit. */",
         "#ifndef PROTO_TAB_H", "#define PROTO_TAB_H", "",
         f"#define PROTO_VERSION {proto.VERSION}",
         f"#define LEN_VAR 0x{VAR:02X}   /* [type][len][payload] */",
         f"#define LEN_BAD 0x{BAD:02X}   /* not a server message type */", "",
         "/* server -> client payload length, indexed by type - 'a' (defined in proto.c) */",
         "extern const unsigned char len_tab[26];",
         "#define LEN_TAB { \\"]
    row = []
    for c in range(ord("a"), ord("z") + 1):
        if c in proto.SERVER_VAR:
            v = "LEN_VAR"
        elif c in proto.SERVER_LEN:
            v = str(proto.SERVER_LEN[c])
        else:
            v = "LEN_BAD"
        row.append(f"/*{chr(c)}*/ {v}")
    for i in range(0, 26, 6):
        L.append("    " + ", ".join(row[i:i + 6]) + ", \\")
    L += ["}", "", "/* client -> server payload lengths */"]
    for t, n in proto.CLIENT_LEN.items():
        L.append(f"#define CLEN_{t.decode()} {n}")
    L += ["", "/* seat kinds (n), fault codes (f) */",
          f"#define SEAT_EMPTY {proto.EMPTY}", f"#define SEAT_HUMAN {proto.HUMAN}",
          f"#define SEAT_AWAY {proto.AWAY}", f"#define SEAT_BOT {proto.BOT}",
          f"#define F_VERSION {proto.F_VERSION}", f"#define F_FULL {proto.F_FULL}",
          f"#define F_TOKEN {proto.F_TOKEN}", f"#define F_ILLEGAL {proto.F_ILLEGAL}",
          "", "#endif", ""]
    return "\n".join(L)


def main():
    if sys.argv[1:] != ["h"]:
        sys.exit(__doc__)
    sys.stdout.write(gen_h())


if __name__ == "__main__":
    main()
