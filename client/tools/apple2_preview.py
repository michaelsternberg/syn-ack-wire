#!/usr/bin/env python3
"""Show a game screen as an Apple II would draw it on an NTSC colour monitor, three ways.

Usage:
    apple2_preview.py ATARI_ROM APPLE_ROM DUMP OUTDIR [--scale N]

ATARI_ROM is an XL/XE OS image (16 KB, the font is at $E000). APPLE_ROM is the
enhanced IIe video ROM, 342-0265-a.chr, or a MAME apple2ee.zip holding it. Neither
is part of the repository. DUMP is an Atari screen dump (960 bytes, internal codes,
as render_screen.py takes).

It writes to OUTDIR:
    apple-hires.png  hi-res, the Apple font: 1-pixel strokes, which artifact into colour
    atari-hires.png  hi-res, the Atari font with its column 0 dropped to fit 7 pixels
    apple-text.png   text mode, the Apple font: no colour burst, so a clean monochrome
    compare.png      the three side by side
    closeup.png      a few rows of both hi-res fonts up close, and a line of the wide glyphs

The NTSC model: a hi-res pixel is 2 samples at 14.318 MHz (4 to a colour cycle), the
high bit is 0 everywhere, and each sample is decoded from the 4 around it (luma their
mean, chroma their first harmonic), with the phase set so a lone even pixel is violet.
The Apple has no yen glyph: hi-res gets a hand-drawn one, text mode a "$"."""
import argparse
import cmath
import math
import os
import zipfile
from PIL import Image, ImageDraw

YEN = bytes([0x00, 0x66, 0x66, 0x3C, 0x7E, 0x18, 0x7E, 0x18])  # display_text.c
# The Apple-style yen, 1-pixel strokes; bit 0 is the leftmost pixel
APPLE_YEN = bytes([0x11, 0x0A, 0x04, 0x1F, 0x04, 0x1F, 0x04, 0x00])
WIDE_TEST = "MAMBA WIRE M&W ^#*_ mw"


def atari_font(path):
    f = bytearray(open(path, "rb").read()[0x2000:0x2400])
    f[0x3C * 8:0x3C * 8 + 8] = YEN
    return f


def apple_rom(path):
    if path.endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            name = next(n for n in z.namelist() if n.endswith("342-0265-a.chr"))
            return z.read(name)
    return open(path, "rb").read()


def to_apple(b):
    """An Atari glyph row (bit 7 = column 0) without column 0, bit 0 leftmost."""
    return sum(1 << i for i in range(7) if b & (0x40 >> i))


def atari_glyphs(font):
    return [bytes(to_apple(b) for b in font[c * 8:c * 8 + 8]) for c in range(128)]


def apple_glyphs(rom, yen):
    """Internal code -> 8 rows. The normal set is stored inverted at $80-$FF, by ASCII."""
    def ascii_glyph(a):
        return bytes(~rom[(0x80 | a) * 8 + r] & 0x7F for r in range(8))
    out = []
    for c in range(128):
        if c == 0x3C:
            out.append(yen if isinstance(yen, bytes) else ascii_glyph(yen))
        elif c < 0x40:
            out.append(ascii_glyph(c + 0x20))
        elif c < 0x60:
            out.append(bytes(8))        # ATASCII graphics: none on these screens
        else:
            out.append(ascii_glyph(c))
    return out


def bitmap(glyphs, scr, cols=40):
    """Screen codes -> 192 (or fewer) lines of 7*cols pixels, 1 = lit."""
    lines = []
    for r in range(len(scr) // cols):
        for k in range(8):
            line = []
            for c in range(cols):
                b = scr[r * cols + c]
                bits = glyphs[b & 0x7F][k] ^ (0x7F if b & 0x80 else 0)
                line += [(bits >> i) & 1 for i in range(7)]
            lines.append(line)
    return lines


def yiq(y, i, q):
    rgb = (y + 0.956 * i + 0.621 * q, y - 0.272 * i - 0.647 * q, y - 1.106 * i + 1.703 * q)
    return tuple(max(0, min(255, round(v * 255))) for v in rgb)


def decode_table(phi):
    """(window of 4 samples as bits, sample phase) -> RGB."""
    table = {}
    for n in range(4):
        for pat in range(16):
            s = [(pat >> j) & 1 for j in range(4)]   # samples n-1 .. n+2
            y = sum(s) / 4
            c = sum(s[j] * cmath.exp(-1j * math.pi / 2 * (n - 1 + j)) for j in range(4)) / 2
            c *= cmath.exp(1j * phi)
            table[n, pat] = yiq(y, c.real, c.imag)
    return table


def calibrate():
    """The phase that makes a lone even pixel violet, Apple's (255, 68, 253)."""
    target = (255, 68, 253)
    tm = sum(target) / 3
    best = None
    for deg in range(360):
        phi = math.radians(deg)
        rgb = decode_table(phi)[0, 0b0110]           # samples 0, 1 lit: pixel 0
        m = sum(rgb) / 3
        dot = sum((a - m) * (b - tm) for a, b in zip(rgb, target))
        if best is None or dot > best[0]:
            best = (dot, phi)
    return best[1]


def ntsc(lines, table):
    w = len(lines[0]) * 2
    img = Image.new("RGB", (w, len(lines)))
    px = img.load()
    for y, line in enumerate(lines):
        s = [0] + [p for p in line for _ in (0, 1)] + [0, 0]
        for n in range(w):
            pat = s[n] | s[n + 1] << 1 | s[n + 2] << 2 | s[n + 3] << 3
            px[n, y] = table[n & 3, pat]
    return img


def mono(lines):
    w = len(lines[0]) * 2
    img = Image.new("RGB", (w, len(lines)))
    px = img.load()
    for y, line in enumerate(lines):
        for x, p in enumerate(line):
            if p:
                px[2 * x, y] = px[2 * x + 1, y] = (255, 255, 255)
    return img


def scaled(img, sx, sy):
    return img.resize((img.width * sx, img.height * sy), Image.NEAREST)


def labelled(images, labels, gap=12, top=22):
    w = sum(i.width for i in images) + gap * (len(images) - 1)
    h = max(i.height for i in images) + top
    out = Image.new("RGB", (w, h), (40, 40, 40))
    d = ImageDraw.Draw(out)
    x = 0
    for img, text in zip(images, labels):
        d.text((x + 4, 4), text, fill=(220, 220, 220))
        out.paste(img, (x, top))
        x += img.width + gap
    return out


def stacked(images, labels, gap=12, top=22):
    w = max(i.width for i in images)
    h = sum(i.height + top for i in images) + gap * (len(images) - 1)
    out = Image.new("RGB", (w, h), (40, 40, 40))
    d = ImageDraw.Draw(out)
    y = 0
    for img, text in zip(images, labels):
        d.text((4, y + 4), text, fill=(220, 220, 220))
        out.paste(img, (0, y + top))
        y += img.height + top + gap
    return out


def text_codes(s):
    return bytes(ord(ch) - 0x20 if " " <= ch <= "_" else ord(ch) for ch in s)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("atari_rom")
    ap.add_argument("apple_rom")
    ap.add_argument("dump")
    ap.add_argument("outdir")
    ap.add_argument("--scale", type=int, default=2, help="output pixels per sample (default 2)")
    a = ap.parse_args()

    afont = atari_font(a.atari_rom)
    arom = apple_rom(a.apple_rom)
    scr = open(a.dump, "rb").read()
    g_atari = atari_glyphs(afont)
    g_apple = apple_glyphs(arom, APPLE_YEN)
    g_text = apple_glyphs(arom, ord("$"))

    # What dropping column 0 costs, and which glyphs reach column 7
    text = list(range(0x00, 0x40)) + list(range(0x60, 0x80))
    lost = [c for c in text if any(b & 0x80 for b in afont[c * 8:c * 8 + 8])]
    wide = [c for c in text if any(b & 0x01 for b in afont[c * 8:c * 8 + 8])]
    show = lambda cs: " ".join(chr(c + 0x20) if c < 0x40 else chr(c) for c in cs)
    print("column 0 dropped from:", show(lost) or "none")
    print("reach column 7:      ", show(wide))
    used = sorted({b & 0x7F for b in scr} & set(wide))
    print("wide glyphs on this screen:", show(used) or "none")

    phi = calibrate()
    table = decode_table(phi)
    print("lone pixels: even", table[0, 0b0110], "odd", table[2, 0b0110], "pair", table[0, 0b1111])

    s = a.scale
    os.makedirs(a.outdir, exist_ok=True)
    panels = {
        "apple-hires": ntsc(bitmap(g_apple, scr), table),
        "atari-hires": ntsc(bitmap(g_atari, scr), table),
        "apple-text": mono(bitmap(g_text, scr)),
    }
    for name, img in panels.items():
        scaled(img, s, 2 * s).save(os.path.join(a.outdir, name + ".png"))
    labelled([scaled(i, 1, 2) for i in panels.values()],
             ["Hi-res, Apple font (colour NTSC)", "Hi-res, Atari font (colour NTSC)",
              "Text mode, Apple font (no burst)"]).save(os.path.join(a.outdir, "compare.png"))

    # Close-up: the title bar, the seats' heading and two seats, the left 20 columns, then the wide glyphs
    rows = [0, 1, 12, 13, 14]
    near = b"".join(scr[r * 40:r * 40 + 20] for r in rows) + text_codes(WIDE_TEST.ljust(20)[:20])
    close = [scaled(ntsc(bitmap(g, near, 20), table), 4, 8) for g in (g_apple, g_atari)]
    stacked(close, ["Apple font", "Atari font, column 0 dropped"]).save(
        os.path.join(a.outdir, "closeup.png"))
    print("wrote", ", ".join(n + ".png" for n in list(panels) + ["compare", "closeup"]), "in", a.outdir)


if __name__ == "__main__":
    main()
