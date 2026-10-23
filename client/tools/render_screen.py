#!/usr/bin/env python3
"""Draw an Atari screen dump as the client shows it, for the manual's captures.

Usage:
    render_screen.py ROM DUMP OUT.png [--playfield] [--ask] [--scale N]

ROM is an XL/XE OS image (16 KB, the font is at $E000); it is not part of the
repository. DUMP is 960 bytes of screen memory, internal codes, as the autoplay
client saves it (client/tools/run-test.sh with DUMPDIR). The client's own yen
glyph replaces "\\", as display_text.c does.

    --playfield   the game screen's panel and bands (display_text.c, dli.s): the
                  Grid panel behind columns 2-13 of rows 2-10, the seats' band
                  (rows 13-18); without it, the title screen's plain background
    --ask         the purple band behind rows 22-23: a decision is waiting
    --scale N     output pixels per Atari pixel (default 4)

The title's static (player/missile graphics) is not drawn; the colors are close
to the NTSC palette, not exact."""
import argparse
from PIL import Image

YEN = bytes([0x00, 0x66, 0x66, 0x3C, 0x7E, 0x18, 0x7E, 0x18])  # display_text.c
BG = (0x00, 0x2E, 0x00)         # COLOR2 $C0
PANEL = (0x14, 0x48, 0x12)      # $C2: the Grid panel and the seats' band
PURPLE = (0x3C, 0x10, 0x6C)     # $60
FG = (0xB4, 0xF0, 0xA8)         # luminance $C (COLOR1) on green
FG_PURPLE = (0xE0, 0xB8, 0xFF)  # the same on purple
BORDER = (0, 0, 0)              # COLOR4
BX, BY = 10, 8                  # border around the 320 x 192 screen, in Atari pixels


def background(r, c, playfield, ask):
    if ask and r >= 22:
        return PURPLE
    if playfield and (2 <= r <= 10 and 2 <= c <= 13 or 13 <= r <= 18):
        return PANEL
    return BG


def render(font, scr, playfield, ask, z):
    img = Image.new("RGB", (320 + 2 * BX, 192 + 2 * BY), BORDER)
    px = img.load()
    for r in range(24):
        for c in range(40):
            b = scr[r * 40 + c]
            glyph = font[(b & 0x7F) * 8:(b & 0x7F) * 8 + 8]
            back = background(r, c, playfield, ask)
            fore = FG_PURPLE if back == PURPLE else FG
            for y in range(8):
                for x in range(8):
                    on = (glyph[y] >> (7 - x) & 1) ^ (b >> 7)
                    px[BX + c * 8 + x, BY + r * 8 + y] = fore if on else back
    return img.resize((img.width * z, img.height * z), Image.NEAREST)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("rom")
    ap.add_argument("dump")
    ap.add_argument("out")
    ap.add_argument("--playfield", action="store_true")
    ap.add_argument("--ask", action="store_true")
    ap.add_argument("--scale", type=int, default=4)
    a = ap.parse_args()
    rom = open(a.rom, "rb").read()
    font = bytearray(rom[0x2000:0x2400])
    font[0x3C * 8:0x3C * 8 + 8] = YEN
    scr = open(a.dump, "rb").read()
    if len(scr) != 960:
        ap.error(f"{a.dump}: {len(scr)} bytes, not 960")
    render(font, scr, a.playfield, a.ask, a.scale).save(a.out)


if __name__ == "__main__":
    main()
