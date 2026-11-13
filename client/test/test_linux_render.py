"""tools/linux_render.py: a cell dump (test/linux_dump.c) to a PNG.   python3 -m unittest test.test_linux_render"""
import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import linux_render as lr


def dump(cells):
    out = bytearray()
    for r in range(24):
        for c in range(80):
            ch, fg, bg, attr = cells.get((r, c), (32, 8, 0, 0))
            out += struct.pack("<IBBBx", ch, fg, bg, attr)
    return bytes(out)


class RenderTest(unittest.TestCase):
    def test_palette_matches_screen_c(self):
        pal = lr.palette()
        self.assertEqual(len(pal), 21)
        self.assertEqual(pal[0], (0x26, 0x2B, 0x33))
        self.assertEqual(pal[4], (0x33, 0xDD, 0xEE))
        self.assertEqual(pal[20], (0xAA, 0x88, 0xFF))

    def test_cell_background_is_the_palette_colour(self):
        data = dump({(3, 7): (ord("X"), 12, 5, 0)})
        img = lr.render(data)
        self.assertEqual(img.size, (80 * lr.CW, 24 * lr.CH))
        self.assertEqual(img.getpixel((7 * lr.CW + 1, 3 * lr.CH + 1)), lr.palette()[5])
        self.assertEqual(img.getpixel((1, 1)), lr.palette()[0])

    def test_glyph_ink_uses_the_foreground(self):
        img = lr.render(dump({(0, 0): (0x2588, 4, 0, 0)}))          # a full block
        self.assertEqual(img.getpixel((lr.CW // 2, lr.CH // 2)), lr.palette()[4])

    def test_bad_size_is_refused(self):
        with self.assertRaises(SystemExit):
            lr.render(b"\0" * 10)

    def test_png_written(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "s.bin")
            open(src, "wb").write(dump({}))
            out = os.path.join(d, "o.png")
            lr.main([src, out])
            self.assertTrue(open(out, "rb").read(4) == b"\x89PNG")


if __name__ == "__main__":
    unittest.main()
