#!/usr/bin/env python3
"""A screen dump of the linux client (test/linux_dump.c) as a PNG, to compare with the reference by eye.

Usage:
    linux_render.py DUMP.bin OUT.png

The dump is 24 x 80 records of { uint32 code point, uint8 fg, bg, attr, pad }, little-endian (screen.h's nl_cell);
the colours are palette indices, read from the table in src/linux/screen.c so the picture and the program agree.
A cell is CW x CH pixels. Needs Pillow and a monospace TTF with box drawing."""
import os
import re
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

CW, CH = 16, 34
ROWS, COLS = 24, 80
FONTS = ["/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf", "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
         "/usr/share/fonts/TTF/DejaVuSansMono.ttf"]
BOLD = ["/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Bold.ttf", "/usr/share/fonts/liberation/LiberationMono-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf"]
SCREEN_C = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "linux", "screen.c")


def palette():
    text = open(SCREEN_C).read()
    body = text[text.index("nl_pal[") if "nl_pal[" in text else text.index("0x262b33") - 40:]
    vals = re.findall(r"0x([0-9a-fA-F]{6})\b", body)[:21]
    return [(int(v[0:2], 16), int(v[2:4], 16), int(v[4:6], 16)) for v in vals]


def font(paths, size):
    for p in paths:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def render(data):
    if len(data) != ROWS * COLS * 8:
        sys.exit("linux_render.py: a dump is %d bytes, not %d" % (len(data), ROWS * COLS * 8))
    pal = palette()
    img = Image.new("RGB", (COLS * CW, ROWS * CH), pal[0])
    d = ImageDraw.Draw(img)
    f, fb = font(FONTS, 26), font(BOLD, 26)
    for r in range(ROWS):
        for c in range(COLS):
            ch, fg, bg, attr = struct.unpack_from("<IBBBx", data, (r * COLS + c) * 8)
            x, y = c * CW, r * CH
            d.rectangle([x, y, x + CW - 1, y + CH - 1], fill=pal[bg % len(pal)])
            if ch == 0x2588:
                d.rectangle([x, y, x + CW - 1, y + CH - 1], fill=pal[fg % len(pal)])
            elif ch == 0x2580:
                d.rectangle([x, y, x + CW - 1, y + CH // 2 - 1], fill=pal[fg % len(pal)])
            elif ch > 32:
                d.text((x + CW // 2, y + CH // 2), chr(ch), font=fb if attr & 1 else f, fill=pal[fg % len(pal)], anchor="mm")
    return img


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    render(open(argv[0], "rb").read()).save(argv[1])


if __name__ == "__main__":
    main(sys.argv[1:])
