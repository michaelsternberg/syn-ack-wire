#!/usr/bin/env python3
"""Draw an Apple II text screen dump as the screen looks, for the manual's captures; or show it as text.

Usage:
    render_apple2.py ROM DUMP OUT.png [--scale N]
    render_apple2.py --text DUMP...

ROM is the enhanced IIe video ROM, 342-0265-a.chr (the IIc and IIgs show the same primary set), or a MAME
apple2ee.zip holding it; it is not part of the repository. DUMP is 1024 bytes of text page 1 ($0400-$07FF) in
the screen's own interleaved order, as `make pc-apple2` saves it (tools/capture.sh with PLATFORM=apple2).

Normal video is white on black; inverse is black on white; flashing characters are drawn in their inverse
phase. --text prints inverse and flashing letters in lower case, as screen2txt.py does for the Atari."""
import argparse
import zipfile

FG = (0xE8, 0xE8, 0xE8)     # a white phosphor, slightly soft
BG = (0x10, 0x10, 0x10)
BORDER = (0, 0, 0)
BX, BY = 10, 8              # border around the 280 x 192 screen, in Apple pixels


def row_offset(r):
    return (r & 7) * 0x80 + (r >> 3) * 40


def cell(b):
    """A screen byte -> (ASCII character, inverse): normal $80-$FF, inverse $00-$3F, flashing $40-$7F."""
    if b >= 0x80:
        c, inv = b & 0x7F, False
    else:
        c, inv = b & 0x3F, True
    if c < 0x20:
        c += 0x40                       # @ A-Z [ \\ ] ^ _
    return chr(c), inv


def rows(dump):
    return [[cell(b) for b in dump[row_offset(r):row_offset(r) + 40]] for r in range(24)]


def text(dump):
    return ["".join(ch.lower() if inv and ch.isalpha() else ch for ch, inv in line) for line in rows(dump)]


def apple_rom(path):
    if path.endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            return z.read(next(n for n in z.namelist() if n.endswith("342-0265-a.chr")))
    return open(path, "rb").read()


def render(rom, dump, scale):
    from PIL import Image
    img = Image.new("RGB", (280 + 2 * BX, 192 + 2 * BY), BORDER)
    px = img.load()
    for x in range(280):
        for y in range(192):
            px[BX + x, BY + y] = BG
    for r, line in enumerate(rows(dump)):
        for c, (ch, inv) in enumerate(line):
            for y in range(8):
                bits = ~rom[(0x80 | ord(ch)) * 8 + y] & 0x7F    # the normal glyph, bit 0 leftmost
                if inv:
                    bits ^= 0x7F
                for x in range(7):
                    if bits >> x & 1:
                        px[BX + c * 7 + x, BY + r * 8 + y] = FG
    return img.resize((img.width * scale, img.height * scale), Image.NEAREST)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--text", action="store_true", help="print each DUMP as text instead")
    ap.add_argument("--scale", type=int, default=4, help="output pixels per Apple pixel (default 4)")
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()
    if a.text:
        for f in a.files:
            print("+" + "-" * 40 + "+")
            for line in text(open(f, "rb").read()):
                print("|" + line + "|")
            print("+" + "-" * 40 + "+")
        return
    if len(a.files) != 3:
        ap.error("need ROM DUMP OUT.png")
    rom, dump, out = a.files
    render(apple_rom(rom), open(dump, "rb").read(), a.scale).save(out)


if __name__ == "__main__":
    main()
