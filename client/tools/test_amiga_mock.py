"""Tests for amiga_mock.py: every mockup screen fits the Amiga's 32 colours, and the palette header is what the C code needs.
   python3 -m unittest tools/test_amiga_mock.py   (from client/)"""
import os, re, subprocess, sys, tomllib, unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import amiga_mock as m

FONT = os.path.join(HERE, "..", "src", "nes", "font.txt")
THEME = os.path.join(HERE, "..", "..", "theme", "theme.toml")
m.load_font(FONT)

SCREENS = [("title", m.title), ("playfield", m.playfield), ("buy", m.buy), ("menu", m.menu),
           ("playfield_ntsc", m.playfield_ntsc), ("buy_ntsc", m.buy_ntsc)]


def header():
    return subprocess.run([sys.executable, os.path.join(HERE, "amiga_mock.py"), "--header"],
                          capture_output=True, text=True, check=True).stdout


class MockTests(unittest.TestCase):
    def test_every_screen_within_32_colours(self):
        for name, fn in SCREENS:
            self.assertLessEqual(fn().ncolors(), 32, name)

    def test_ntsc_screens_are_320x200(self):
        for fn in (m.playfield_ntsc, m.buy_ntsc):
            self.assertEqual(fn().im.size, (320, 200))
        self.assertEqual(m.playfield().im.size, (320, 256))

    def test_palette_header_has_32_entries_each_12_bit(self):
        h = header()
        body = re.search(r"amiga_palette\[32\] = \{(.*?)\};", h, re.S).group(1)
        entries = re.findall(r"0x([0-9A-Fa-f]{4})", body)
        self.assertEqual(len(entries), 32)
        for e in entries:
            self.assertEqual(e[0], "0")          # 0x0RGB

    def test_syndicate_pens_are_contiguous_and_named_from_the_theme(self):
        h = header()
        pens = dict((k, int(v)) for k, v in re.findall(r"#define PEN_(\w+)\s+(\d+)", h))
        self.assertEqual(pens["SYN_DIM"] - pens["SYN"], 12)      # 7 bright, the 3 sprite registers, then the dim
        theme = tomllib.load(open(THEME, "rb"))
        letters = "".join(s["letter"] for s in theme["syndicate"])
        self.assertIn("syndicates: " + letters, h)

    def test_sprite_registers_17_to_19_are_reserved(self):
        h = header()
        self.assertRegex(h, r"#define PEN_SPRITE\s+17")

    def test_the_tile_look_is_gone(self):
        self.assertFalse(hasattr(m, "alt"))


if __name__ == "__main__":
    unittest.main()
