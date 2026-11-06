#!/usr/bin/env python3
"""Draw an Amiga screen dump (test/amiga_dump.c: the autoplay build's, in the rig's SYNACK:d/) as a PNG.

    render_amiga.py DUMP OUT.png [--scale N]     each Amiga pixel N x N (default 2: 640 x 512 for PAL)
    render_amiga.py --text DUMP                  the prompt strip's text and the title's status, as the dump has them

A dump is "SAWD", rows, bytes per row, depth, 0, then 32 colours (0x0RGB), the strip's and the status's text
(48 bytes each) and the bitplanes, all big-endian. The manual places a callout on a cell of 8 x 8 Amiga pixels
(docs/manual/amiga/platform.tex), so at the default scale a cell is 16 x 16 image pixels."""
import argparse
import struct

from PIL import Image


def parse(d):
    if d[:4] != b"SAWD":
        raise SystemExit("not an Amiga screen dump")
    rows, bpr, depth = struct.unpack(">HHB", d[4:9])
    pal = struct.unpack(">32H", d[10:74])
    strip = d[74:122].split(b"\0")[0].decode("latin-1")
    status = d[122:170].split(b"\0")[0].decode("latin-1")
    planes = [d[170 + p * rows * bpr:170 + (p + 1) * rows * bpr] for p in range(depth)]
    return rows, bpr, depth, pal, strip, status, planes


def text(d):
    """The strip's text, then the status: what tools/capture.sh picks a dump by."""
    return list(parse(d)[4:6])


def render(d, scale=2):
    rows, bpr, depth, pal, _, _, planes = parse(d)
    rgb = [((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17) for c in pal]
    width = 320
    img = Image.new("RGB", (width, rows))
    px = img.load()
    for y in range(rows):
        base = y * bpr
        for x in range(width):
            byte, bit = base + (x >> 3), 7 - (x & 7)
            pen = 0
            for p in range(depth):
                pen |= (planes[p][byte] >> bit & 1) << p
            px[x, y] = rgb[pen]
    return img.resize((width * scale, rows * scale), Image.NEAREST)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--text", action="store_true")
    a = ap.parse_args()
    data = open(a.dump, "rb").read()
    if a.text:
        print("\n".join(text(data)))
    else:
        render(data, a.scale).save(a.out)
