#!/usr/bin/env python3
"""Tests for help_amiga.py: python3 tools/test_help_amiga.py"""
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "help_amiga.py")
PAGES = os.path.join(HERE, "..", "help", "amiga", "screen")
URL = "HTTPS://SYN-ACK-WIRE.FLY.DEV/AMIGA"
sys.path.insert(0, HERE)
import qr  # noqa: E402


def run(out, paths, url=URL):
    return subprocess.run([sys.executable, TOOL, "--qr", url, out] + paths, capture_output=True, text=True)


def good(n, m, extra=""):
    return "{ A PAGE%s%*d/%d }\n%s" % ("", 30, n, m, extra)


class Pages(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.out = os.path.join(self.d, "help_tab.h")

    def write(self, n, text):
        p = os.path.join(self.d, "HELP%02d.TXT" % n)
        open(p, "w").write(text)
        return p

    def test_a_good_page_checks(self):
        r = run(self.out, [self.write(1, good(1, 1, "hello\n"))])
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_a_41_character_line_is_refused(self):
        self.assertNotEqual(run(self.out, [self.write(1, good(1, 1, "x" * 41 + "\n"))]).returncode, 0)

    def test_a_25th_line_is_refused(self):
        self.assertNotEqual(run(self.out, [self.write(1, good(1, 1, "x\n" * 24))]).returncode, 0)

    def test_a_missing_page_number_is_refused(self):
        self.assertNotEqual(run(self.out, [self.write(1, good(1, 2)), self.write(3, good(2, 2))]).returncode, 0)

    def test_a_heading_with_the_wrong_number_is_refused(self):
        self.assertNotEqual(run(self.out, [self.write(1, good(1, 2)), self.write(2, good(3, 2))]).returncode, 0)

    def test_the_real_pages_make_a_header_with_their_qr(self):
        paths = sorted(os.path.join(PAGES, f) for f in os.listdir(PAGES) if f.startswith("HELP"))
        r = run(self.out, paths)
        self.assertEqual(r.returncode, 0, r.stderr)
        h = open(self.out).read()
        self.assertIn("#define HELP_PAGES 11", h)
        n = int(re.search(r"#define HELP_QR_N (\d+)", h).group(1))
        self.assertEqual(int(re.search(r"#define HELP_QR_PAGE (\d+)", h).group(1)), 10)
        body = re.search(r"help_qr\[\] = \{([^}]*)\}", h).group(1)
        bits = bytes(int(x, 0) for x in body.replace("\n", " ").split(",") if x.strip())
        m, _, _ = qr.encode(URL, "H", 3), 3, "H"
        # the tool picks the same version and level as the MS-DOS pages (highest level that fits); compare module by module
        for y in range(n):
            for x in range(n):
                i = y * n + x
                self.assertEqual(bool(bits[i // 8] >> (7 - i % 8) & 1), bool(m[y][x]), (x, y))

    def test_text_rows_are_length_prefixed_with_reverse_toggles(self):
        r = run(self.out, [self.write(1, "{ AB }\nplain\n")])
        self.assertEqual(r.returncode, 0, r.stderr)
        h = open(self.out).read()
        body = re.search(r"help_data\[\] = \{([^}]*)\}", h).group(1)
        data = bytes(int(x, 0) for x in body.replace("\n", " ").split(",") if x.strip())
        self.assertEqual(data[0], 1 + 4 + 1)                # toggle, " AB ", toggle (the heading row)
        self.assertEqual(data[1], 1)
        self.assertEqual(data[2:6], b" AB ")
        self.assertEqual(data[6], 1)


if __name__ == "__main__":
    unittest.main()
