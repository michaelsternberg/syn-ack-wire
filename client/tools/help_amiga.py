#!/usr/bin/env python3
"""The Amiga instruction pages as a C header, build/amiga/help_tab.h (the Makefile's rule), compiled into the program.

Usage:
    help_amiga.py [--qr URL] OUT.h HELP01.TXT HELP02.TXT ...

The pages are the MS-DOS ones' kind (help_msdos.py checks them: at most 24 lines of 40 characters, `{`..`}` in
reverse, numbering and `n/m` headings, [QR] marker on lines 1-20) and so is the QR code (tools/qr.py). Each text row
becomes a length byte and its characters, a 0x01 in them toggling reverse video, "\\" the yen sign (0xA5 in Topaz);
a page's 24 rows follow each other, help_off[p] is where page p starts. The QR code is one bit a module, row by row,
most significant bit first: the Amiga draws it as pixels, with its own white border."""
import sys

import help_msdos as hm

TOGGLE = 1
YEN = 0xA5


def rows(lines, skip_to=0):
    out = bytearray()
    for r in range(24):
        line = lines[r] if r < len(lines) and r >= skip_to else ""
        b, rev = bytearray(), False
        for ch in line:
            if ch in "{}":
                b.append(TOGGLE)
                rev = ch == "{"
                continue
            b.append(YEN if ch == "\\" else ord(ch))
        while b and b[-1] == ord(" "):               # trailing blanks cost bytes and show nothing
            b.pop()
        out.append(len(b))
        out += b
    return out


def c_array(name, data, ctype="unsigned char"):
    L = ["static const %s %s[] = {" % (ctype, name)]
    for i in range(0, len(data), 16):
        L.append("    " + ", ".join(str(x) for x in data[i:i + 16]) + ",")
    L.append("};")
    return "\n".join(L)


def main():
    args = sys.argv[1:]
    url = None
    if args[:1] == ["--qr"]:
        url, args = args[1], args[2:]
    out, paths = args[0], args[1:]
    hm.check(paths, url)
    data, offs, qr_page, qr_bits, qr_n = bytearray(), [], 0, bytearray(), 0
    for n, path in enumerate(paths, 1):
        lines = open(path).read().rstrip("\n").split("\n")
        code = any(line.strip() == "[QR]" for line in lines)
        offs.append(len(data))
        data += rows(lines, 20 if code else 0)
        if code:
            m, _, _ = hm.qr_code(url)
            qr_page, qr_n = n, len(m)
            qr_bits = bytearray((qr_n * qr_n + 7) // 8)
            for y in range(qr_n):
                for x in range(qr_n):
                    if m[y][x]:
                        i = y * qr_n + x
                        qr_bits[i // 8] |= 0x80 >> (i % 8)
    h = ["/* GENERATED from help/amiga/screen by tools/help_amiga.py -- do not edit. */",
         "#ifndef HELP_TAB_H", "#define HELP_TAB_H", "",
         "#define HELP_PAGES %d" % len(paths),
         "#define HELP_QR_PAGE %d" % qr_page,
         "#define HELP_QR_N %d" % qr_n, "",
         c_array("help_off", offs, "unsigned short"), "",
         c_array("help_data", data), "",
         c_array("help_qr", qr_bits or b"\0"), "", "#endif", ""]
    open(out, "w").write("\n".join(h))
    print("%s: %d pages, %d bytes of text, a %dx%d QR code" % (out, len(paths), len(data), qr_n, qr_n))


if __name__ == "__main__":
    main()
