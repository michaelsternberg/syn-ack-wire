#!/usr/bin/env python3
"""teaser.py -- the SYN-ACK WIRE Intellivision teaser: 2:00, 1920 x 1080, 60 fps.

usage: teaser.py OUT.mp4              the video, with build/music.wav (psg.py) for its sound
       teaser.py --still T [T...]      PNG stills at T seconds: build/still_T.png
       teaser.py --check               the captions' characters, and the clip pictures against render_intv.py's

The picture is a canvas of Intellivision units (one pixel across, one scanline down), 240 x 270, each 8 x 4
video pixels, as M.U.L.E.'s Intellivision intro draws it (vidlib.py). All text is the Intellivision's own font:
the GROM's cards (jzIntv's rom/grom.bin), with the game's yen card; names and terms come from theme/theme.toml.
The game is the real client in jzIntv: clips.py's dumps, drawn as render_intv.py draws them, at their own speed.
The scenes are cut on song.py's bars; a 4-bar scene is 448 of the console's frames.
"""
import glob
import hashlib
import os
import sys
import tomllib
from bisect import bisect_right

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
BUILD = os.path.join(HERE, "build")
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "client", "tools"))
import render_intv  # noqa: E402
import song  # noqa: E402
import vidlib  # noqa: E402
from vidlib import Canvas, typed, blink  # noqa: E402

W, H, SX, SY = 240, 270, 8, 4
(BLACK, BLUE, RED, TAN, DKGREEN, GREEN, YELLOW, WHITE,
 GREY, CYAN, ORANGE, BROWN, PINK, LTBLUE, YELGREEN, PURPLE) = range(16)
PAL = np.array(render_intv.PAL, dtype=np.uint8)
GROM = np.unpackbits(np.frombuffer(open(os.path.join(render_intv.JZ, "rom", "grom.bin"), "rb").read(),
                                   dtype=np.uint8)).reshape(256, 8, 8).astype(bool)
GLYPH = {chr(32 + i): GROM[i] for i in range(96)}         # ASCII 32-126, and 127 the block
GLYPH["¥"] = vidlib.bitmask([0x82, 0x44, 0x28, 0xFE, 0x10, 0xFE, 0x10, 0x00])   # main.bas's yen card
BLOCK = chr(127)
vidlib.setup(W, H, SX, SY, PAL, GLYPH, WHITE, GREY)

FPS = song.FPS                  # the console's frames a second
BAR = song.BAR_S


def B(bars, six=0):
    """Seconds into a scene: `bars` whole bars and `six` sixteenths."""
    return (bars * song.BAR + six * song.STEP) / FPS


# ---------------------------------------------------------------------------
# Words: the theme's
# ---------------------------------------------------------------------------
TH = tomllib.load(open(os.path.join(REPO, "theme", "theme.toml"), "rb"))
RULES = TH["rules"]
REGION = TH["game"]["region"]
TITLE = TH["game"]["title"]


def t_(key, plural=False):
    v = TH["terms"][key]
    return (v.get("p", v["s"] + "s") if plural else v["s"]).upper()


NODE, NODES, GRID = t_("node"), t_("node", True), t_("grid")
EXPLOITS, SPIKE, HIVE, HIVES = t_("exploit", True), t_("spike"), t_("hive"), t_("hive", True)
NETRUNNERS, JACKIN, INFILTRATION = t_("playertitle", True), t_("jackin"), t_("infiltration")
BROKER, FINDERSCUT, CRYPTO, SPREAD = t_("broker"), t_("finderscut"), t_("coin"), t_("spread")
HIVEJACK, ROOT, SUDO = t_("hivejack"), t_("root"), t_("sudo")
CASHOUT, SWAP, HODL, SAFE = t_("cashout"), t_("swap"), t_("hodl"), t_("safe")
NULLROUTE, MONEY = t_("nullroute"), t_("money")
SEATS_N = "%d-%d" % (RULES["minplayers"], RULES["maxplayers"])
# the syndicates' colours on the Intellivision (display.bas syn_cw: the letter's fg on bg)
SYN_FGBG = [(BLACK, CYAN), (BLACK, TAN), (WHITE, PURPLE), (WHITE, BROWN), (WHITE, RED), (BLACK, GREEN),
            (BLACK, ORANGE)]
SYNS = [(s["letter"], s.get("screen", s["name"]).upper(), fg, bg) for s, (fg, bg) in zip(TH["syndicate"], SYN_FGBG)]
PLATFORMS = [("intv", "INTELLIVISION"), ("atari", "ATARI 8-BIT"), ("apple2", "APPLE II"), ("msdos", "MS-DOS"),
             ("coco", "COCO"), ("nes", "NES"), ("adam", "ADAM")]
SAID = []                       # every string drawn, for --check


def chk(s):
    SAID.append(s)
    return s


# ---------------------------------------------------------------------------
# The game: clips.py's dumps
# ---------------------------------------------------------------------------
_PIC = {}


def picture(d):
    """A dump as render_intv.render() draws it, without the border: 160 x 192 units (each line doubled)."""
    key = hashlib.md5(d).digest()
    if key in _PIC:
        return _PIC[key]
    bt, gram, mode = render_intv.unpack(d)
    gr = np.unpackbits(np.frombuffer(gram, dtype=np.uint8)).reshape(64, 8, 8).astype(bool)
    out = np.zeros((96, 160), dtype=np.uint8)
    for r in range(12):
        for c in range(20):
            w = bt[r * 20 + c]
            cell = out[r * 8:r * 8 + 8, c * 8:c * 8 + 8]
            if not mode and w & 0x1800 == 0x1000:       # colored squares
                cols = [w & 7, (w >> 3) & 7, (w >> 6) & 7, (w >> 9) & 3 | (w >> 11) & 4]
                cols = [0 if k == 7 else k for k in cols]
                cell[:4, :4], cell[:4, 4:], cell[4:, :4], cell[4:, 4:] = cols
                continue
            g = bool(w & 0x800)
            if mode:
                n, fg = (w >> 3) & 0x3F, w & 7
                bg = (w >> 9 & 1) | (w >> 9 & 2) | ((w >> 13) & 1) << 2 | ((w >> 12) & 1) << 3
            else:
                n = (w >> 3) & (0x3F if g else 0xFF)
                fg, bg = (w & 7) | ((w >> 9) & 8 if g else 0), 0
            cell[:] = np.where(gr[n] if g else GROM[n], fg, bg)
    _PIC[key] = PAL[np.repeat(out, 2, axis=0)]
    if len(_PIC) > 4000:
        _PIC.clear()
    return _PIC[key]


class Film:
    """A clip: build/clips/NAME/fNNNNN.bin, NNNNN the console's frame."""

    def __init__(self, name):
        self.files = sorted(glob.glob(os.path.join(BUILD, "clips", name, "f*.bin")))
        if not self.files:
            raise SystemExit("no clip %s: film it first (clips.py)" % name)
        self.nums = [int(os.path.basename(f)[1:6]) for f in self.files]
        self.data = {}

    def dump(self, fno):
        i = max(0, bisect_right(self.nums, fno) - 1)
        if i not in self.data:
            self.data[i] = open(self.files[i], "rb").read()
        return self.data[i]

    def at(self, fno):
        return picture(self.dump(fno))

    def view(self, fno):
        row = render_intv.text(self.dump(fno))[0]
        return "CRYPTO" if row.startswith("CRYPTO") else "SEATS" if REGION in row else "GRID"


_FILMS = {}


def film(name):
    if name not in _FILMS:
        _FILMS[name] = Film(name)
    return _FILMS[name]


def show(cv, name, t, f0, speed=1.0, x=40, y=4):
    """The clip from its frame f0, at the console's pace; returns the frame shown."""
    fno = f0 + t * FPS * speed
    cv.frame(x - 2, y - 2, 164, 196, DKGREEN, 1)
    cv.paste(x, y, film(name).at(fno))
    return fno


# ---------------------------------------------------------------------------
# Text and effects
# ---------------------------------------------------------------------------
def say(cv, t, lines, y=206, step=20, cps=40):
    """Centred lines typed in turn: (t0, text, colour)."""
    for k, (t0, s, c) in enumerate(lines):
        if t >= t0:
            cv.ctext(y + k * step, typed(chk(s), t - t0, cps), c)


def cursor(cv, x, y, t, c=GREEN):
    if blink(t, 2.0):
        cv.text(x, y, BLOCK, c)


def static(cv, k, seed, cols=(WHITE, GREY, GREEN, CYAN, PURPLE, BLUE)):
    """TV static as the title's: coloured squares (4 units by 8), k 0..1 of the screen."""
    if k <= 0:
        return
    rng = np.random.default_rng(seed)
    n = int(k * (W // 4) * (H // 8))
    xs = rng.integers(0, W // 4, n) * 4
    ys = rng.integers(0, H // 8, n) * 8
    cs = rng.integers(0, len(cols), n)
    for x, y, c in zip(xs, ys, cs):
        cv.rect(int(x), int(y), 4, 8, cols[c])


def glitch(cv, k, seed):
    """Bands of the picture pushed sideways, and static: k 0..1."""
    if k <= 0:
        return
    rng = np.random.default_rng(seed)
    for _ in range(1 + int(8 * k)):
        y = int(rng.integers(0, H - 4))
        h = int(rng.integers(2, 14))
        cv.a[y:y + h] = np.roll(cv.a[y:y + h], int(rng.integers(-30, 30) * k), axis=1)
    static(cv, 0.12 * k, seed + 1)


def hit(t, t0, dur=0.25):
    """1 at t0 falling to 0 over dur."""
    return max(0.0, 1.0 - (t - t0) / dur) if t >= t0 else 0.0


def fid(t):
    return int(t * 60)


# ---------------------------------------------------------------------------
# Scenes: (bars, fn(cv, t, d))
# ---------------------------------------------------------------------------
def sc_cold(cv, t, d):
    lines = [(B(0, 4), "> SYN", GREEN), (B(1, 4), "< SYN-ACK", YELLOW), (B(2, 4), "> ACK", GREEN),
             (B(3), "CONNECTED: " + REGION, WHITE)]
    static(cv, 0.01 + 0.02 * (t / d), fid(t))
    last = None
    for k, (t0, s, c) in enumerate(lines):
        if t >= t0:
            shown = typed(chk(s), t - t0, 30)
            cv.text(24, 80 + k * 28, shown, c)
            last = (24 + len(shown) * 8, 80 + k * 28)
    if last:
        cursor(cv, last[0], last[1], t)
    k = max([hit(t, t0, 0.15) * 0.6 for t0, _, _ in lines] + [max(0.0, (t - B(3, 8)) / B(0, 8)) * 0.45])
    glitch(cv, k, fid(t) + 7)


def sc_title(cv, t, d):
    show(cv, "jackin", t, 42, speed=0.9)
    say(cv, t, [(B(0, 8), "FOR THE INTELLIVISION", YELLOW), (B(2), "ONLINE, WITH FUJINET", CYAN)])
    glitch(cv, hit(t, 0, 0.3), fid(t))


def sc_premise(cv, t, d):
    say(cv, t, [(B(0), "SEVEN SYNDICATES.", WHITE), (B(0, 6), "ONE REGION.", WHITE),
                (B(0, 12), "EVERY %s FOR SALE." % NODE, YELLOW)], y=10, step=20, cps=30)
    for i, (letter, name, fg, bg) in enumerate(SYNS):
        t0 = B(1, 8) + i * B(0, 2)
        if t < t0:
            continue
        y = 76 + i * 19
        x = 120 - (len(name) + 2) * 4
        cv.rect(x, y - 1, 8, 18, bg)
        cv.text(x, y, letter, fg)
        cv.text(x + 16, y, typed(chk(name), t - t0, 40), bg)
    say(cv, t, [(B(2, 8), "%s %s ONLINE." % (SEATS_N, NETRUNNERS), GREEN),
                (B(3), "BOTS FILL EMPTY SEATS.", GREEN)], y=218)


def sc_jackin(cv, t, d):
    show(cv, "jackin", t, 1001)
    say(cv, t, [(B(0, 4), "%s AND HOST A TABLE." % JACKIN, WHITE),
                (B(1, 8), "OTHERS JOIN; PICK %s SEATS." % SEATS_N, WHITE)])


TURN = 190                      # the turn clip's first frame (rec); its scenes are 448 frames apart


def sc_spike(cv, t, d):
    show(cv, "turn", t, TURN)
    say(cv, t, [(B(0, 4), "RUN 1 OF YOUR %d %s:" % (RULES["kitsize"], EXPLOITS), WHITE),
                (B(1, 8), "SPIKE A %s ON THE %s." % (NODE, GRID), WHITE),
                (B(2, 8), "YELLOW DIGITS SHOW WHERE.", YELLOW)])


def sc_infil(cv, t, d):
    show(cv, "turn", t, TURN + 448)
    say(cv, t, [(B(0, 2), "TOUCH A LONE %s:" % SPIKE, WHITE),
                (B(0, 12), "%s! %s THE" % (INFILTRATION, BROKER), YELLOW),
                (B(1, 8), "%s, TAKE A %s." % (HIVE, FINDERSCUT), YELLOW)])


def sc_buy(cv, t, d):
    show(cv, "turn", t, TURN + 896)
    say(cv, t, [(B(0, 2), "BACK THE SYNDICATES:", WHITE),
                (B(0, 12), "BUY UP TO %d %s A TURN." % (RULES["maxbuy"], CRYPTO), YELLOW),
                (B(2, 8), "%s %s, PRICES CLIMB." % (HIVES, SPREAD), WHITE)])


def sc_views(cv, t, d):
    fno = show(cv, "turn", t, TURN + 1344)
    v = film("turn").view(fno)
    cv.ctext(204, chk(v), YELLOW, 2, 4)
    say(cv, t, [(B(0, 4), "THREE VIEWS ON THE DISC", WHITE)], y=246)


def sc_jackcard(cv, t, d):
    static(cv, 0.03, fid(t))
    s = chk(HIVEJACK + "!")
    shake = int(4 * hit(t % B(0, 4), 0, 0.15))
    cv.ctext(100 + shake, s, PINK if blink(t, 4.0) else RED, 3, 6)
    glitch(cv, 0.35 + 0.65 * hit(t % B(0, 4), 0, 0.2), fid(t))


def sc_hivejack(cv, t, d):
    show(cv, "hivejack", t, 190)
    if t < B(3, 8):
        say(cv, t, [(B(0, 2), "WHEN %s COLLIDE, THE" % HIVES, WHITE),
                    (B(0, 10), "BIGGER ONE %sS" % HIVEJACK, YELLOW),
                    (B(1, 4), "THE SMALLER.", YELLOW)])
    else:
        say(cv, t - B(3, 8), [(0, "%s AND %s GET PAID." % (ROOT, SUDO), WHITE),
                              (B(1), "%s, %s OR %s." % (CASHOUT, SWAP, HODL), YELLOW)])


KEYS = ["1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"]


def keyboard(cv, x0, y0, down=None):
    """An ECS keyboard, drawn: the key `down` lit (\\n: RTN)."""
    for r, row in enumerate(KEYS):
        x = x0 + r * 6
        for ch in row:
            lit = ch == down
            cv.rect(x, y0 + r * 24, 13, 22, YELLOW if lit else DKGREEN)
            if not lit:
                cv.rect(x + 1, y0 + r * 24 + 1, 11, 20, BLACK)
            cv.text(x + 3, y0 + r * 24 + 3, ch, BLACK if lit else GREEN)
            x += 15
    rx, ry = x0 + 2 * 6 + 9 * 15, y0 + 2 * 24             # RTN, at the end of the home row
    lit = down == "\n"
    cv.rect(rx, ry, 29, 22, YELLOW if lit else DKGREEN)
    if not lit:
        cv.rect(rx + 1, ry + 1, 27, 20, BLACK)
    cv.text(rx + 3, ry + 3, "RTN", BLACK if lit else GREEN)


def sc_ecscard(cv, t, d):
    cv.ctext(8, chk("ECS KEYBOARD"), YELLOW, 2, 4)
    cv.ctext(44, chk("SUPPORTED"), GREEN, 2, 4)
    handle = "ZER0COOL\n"
    t0, each = B(0, 8), B(0, 2)
    n = int(max(0, t - t0) / each) + (1 if t >= t0 else 0)
    n = min(n, len(handle))
    down = handle[n - 1] if 0 < n and (t - t0) % each < each * 0.6 else None
    cv.text(48, 92, chk("HANDLE "), GREEN)
    cv.text(48 + 56, 92, handle[:n].strip(), WHITE)
    cursor(cv, 48 + 56 + len(handle[:n].strip()) * 8, 92, t, WHITE)
    keyboard(cv, 36, 128, down)


def sc_ecshandle(cv, t, d):
    show(cv, "jackin", t, 375)                         # the handle typed from 461, RTN at 596
    say(cv, t, [(B(0, 2), "TYPE YOUR HANDLE ON IT,", WHITE), (B(1, 0), "RTN TO %s." % JACKIN, YELLOW)])


def sc_ecsbuy(cv, t, d):
    show(cv, "ecsbuy", t, 300)
    say(cv, t, [(B(0, 2), "LETTERS PICK SYNDICATES,", WHITE), (B(1, 4), "ARROWS STEP THE VIEWS.", WHITE),
                (B(2, 8), "OR JUST USE THE CONTROLLER.", YELLOW)])


def sc_crossseats(cv, t, d):
    show(cv, "turn", t, TURN + 1390)
    say(cv, t, [(B(0, 1), "PLAY AGAINST ATARI,", WHITE), (B(0, 9), "APPLE II, MS-DOS, COCO,", WHITE),
                (B(1, 1), "NES AND ADAM %s." % NETRUNNERS, YELLOW)], cps=45)


_TILE = {}


def tile_img(plat, bw, bh):
    if plat not in _TILE:
        im = Image.open(os.path.join(REPO, "docs", "manual", plat, "screens", "tied-root.png")).convert("RGB")
        s = min(bw / im.width, bh / im.height)
        im = im.resize((int(im.width * s), int(im.height * s)), Image.LANCZOS)
        _TILE[plat] = np.array(im)
    return _TILE[plat]


def sc_crossgrid(cv, t, d):
    cv.ctext(6, chk("ONE GAME. EVERY MACHINE."), YELLOW)
    bw, bh, gap = 440, 330, 24
    rows = [PLATFORMS[:4], PLATFORMS[4:]]
    for r, row in enumerate(rows):
        x0 = (1920 - len(row) * bw - (len(row) - 1) * gap) // 2
        for c, (plat, label) in enumerate(row):
            i = r * 4 + c
            pop = (t - B(0, 2) - i * B(0, 2)) / 0.15
            if pop <= 0:
                continue
            img = tile_img(plat, bw, bh)
            if pop < 1:
                f = max(0.1, pop)
                img = np.array(Image.fromarray(img).resize((max(1, int(img.shape[1] * f)),
                                                            max(1, int(img.shape[0] * f))), Image.NEAREST))
            h, w = img.shape[:2]
            cx, cy = x0 + c * (bw + gap) + bw // 2, 110 + r * 390 + bh // 2
            frame = np.zeros((h + 8, w + 8, 3), dtype=np.uint8)
            frame[:] = PAL[GREEN if plat == "intv" else DKGREEN]
            frame[4:-4, 4:-4] = img
            cv.over(cx - w // 2 - 4, cy - h // 2 - 4, frame)
            if pop >= 1:
                cv.otext(cx, 110 + r * 390 + bh + 12, chk(label), YELLOW if plat == "intv" else WHITE, 4)
    say(cv, t, [(B(2, 4), "THE SAME TABLES.", GREEN)], y=228)


def sc_endgame(cv, t, d):
    show(cv, "endgame", t, 200)
    if t < B(3):
        say(cv, t, [(B(0, 2), "A %s OF %d+ %s IS" % (HIVE, RULES["safesize"], NODES), WHITE),
                    (B(0, 12), "%s, NEVER TAKEN." % SAFE, YELLOW)])
    else:
        say(cv, t - B(3), [(0, "CALL THE %s." % NULLROUTE, WHITE),
                           (B(0, 8), "THE MOST %s WINS." % MONEY, YELLOW)])


def sc_outro(cv, t, d):
    pic = film("jackin").at(42 + (t * FPS) % 400)
    cv.paste(40, 8, pic[:112])                          # the title's logo, its static running
    glitch(cv, hit(t, 0, 0.4), fid(t))
    lines = [(B(0, 4), "AVAILABLE SOON", 548), (B(0, 12), "IN FUJINET LOBBY", 668)]
    for t0, s, y in lines:
        if t >= t0:
            cv.otext(960, y, typed(chk(s), t - t0, 24), YELLOW, 12)
    if t >= lines[1][0] and blink(t, 2.0) and t - lines[1][0] > len(lines[1][1]) / 24:
        cv.otext(960 + 8 * 96 + 60, 668, BLOCK, YELLOW, 12)
    for k, s in enumerate(["ALSO ON ATARI, APPLE II, MS-DOS,", "COCO, NES AND ADAM"]):
        t0 = B(1, 8) + k * B(0, 8)
        if t >= t0:
            cv.otext(960, 830 + k * 56, typed(chk(s), t - t0, 50), GREEN, 5)


SCENES_BARS = [
    (4, sc_cold), (4, sc_title), (4, sc_premise), (4, sc_jackin),
    (4, sc_spike), (4, sc_infil), (4, sc_buy), (4, sc_views),
    (1, sc_jackcard), (7, sc_hivejack),
    (2, sc_ecscard), (2, sc_ecshandle), (4, sc_ecsbuy),
    (2, sc_crossseats), (4, sc_crossgrid),
    (6, sc_endgame), (4, sc_outro),
]
LENGTH = 120.0
assert sum(b for b, _ in SCENES_BARS) == song.BARS
SCENES = [(b * BAR, fn) for b, fn in SCENES_BARS[:-1]]
SCENES.append((LENGTH - sum(d for d, _ in SCENES), SCENES_BARS[-1][1]))


def check():
    """Draw a frame of every scene every quarter second (collecting the text), check the characters, and the
    pictures against render_intv's."""
    T = 0.0
    while T < LENGTH:
        vidlib.frame_at(SCENES, T)
        T += 0.25
    bad = sorted({ch for s in SAID for ch in s if ch not in GLYPH or ch in "_^`~"})
    print("%d strings; characters missing or misleading in the GROM: %s" % (len(set(SAID)), bad or "none"))
    long_ = sorted({s for s in SAID if len(s) > 28 and "ATARI, APPLE" not in s})
    print("captions over 28 columns:", long_ or "none")
    import tempfile
    for name in ("jackin", "turn", "hivejack", "ecsbuy", "endgame"):
        f = film(name)
        for i in (0, len(f.files) // 3, len(f.files) - 1):
            d = open(f.files[i], "rb").read()
            with tempfile.NamedTemporaryFile(suffix=".png") as tmp:
                render_intv.render(d, tmp.name)
                ref = np.array(Image.open(tmp.name).convert("RGB"))[::2, ::4][8:200, 8:168]
            same = np.array_equal(ref, picture(d))
            print("  %-8s %s: %s" % (name, os.path.basename(f.files[i]), "same" if same else "DIFFERENT"))
            if not same:
                sys.exit(1)
    if bad or long_:
        sys.exit(1)


if __name__ == "__main__":
    if "--check" in sys.argv:
        check()
    else:
        vidlib.run(SCENES, BUILD, sys.argv[1:])
