#!/usr/bin/env python3
"""intvsim.py -- the Intellivision client headless in jzIntv: a scenario of presses, the screen as text
and as pictures. After M.U.L.E.'s (fujinet-multiplayer-mule, clients/intv/tools/intvsim.py, GPL-3.0).

It drives jzIntv's debugger (-d) over a pipe: a breakpoint on frame_isr makes each 'r' one frame.
Pictures are drawn by render_intv.py from BACKTAB, GRAM and the GROM in the mode the game is in (scr_mode: 0
colour stack, with colored squares, 1 foreground/background), as a TV would show them; with DUMPDIR set, each
picture's dump goes there too, scr001.bin and on (tools/capture.sh picks the manual's from them). The controller goes
through input.bas's TEST_HOOK ($8FF0 = $A5, $8FF1 = the controller's byte); the ECS keyboard through
ecskbd.bas's ECS_HOOK ($8FF2: a key as its scanner gives it), when the game found an ECS (ECS=1).

usage: intvsim.py ROM SCENARIO OUTDIR     (SCENARIO "-": stdin; the .sym beside the ROM)
Scenario lines ('#' starts a comment):
    wait N                  run N frames
    snap NAME               OUTDIR/NAME.png, and the screen as text
    text                    the screen as text
    key K                   a keypad key, held 4 frames: 0-9, C (CLEAR), E (ENTER)
    btn T|L|R               a button: top, lower left, lower right
    disc N|E|S|W [FRAMES]   the disc, held 4 frames (or FRAMES)
    hold HEX N              the controller's byte HEX for N frames
    ecs TEXT                typed on the ECS: \\n RTN, \\e ESC, \\b the left arrow, \\r the right one
    until TEXT [MAX]        run until the screen shows TEXT (MAX frames, 3600)
    untilnot TEXT [MAX]     until it doesn't
    untilvar NAME VAL [MAX] until IntyBASIC variable NAME (low byte) is VAL
    dumps PREFIX N [MAX]    run until dump_hook (the autoplay build's) has been called N times, a picture
                            and the text at each: OUTDIR/PREFIX_000.png ...
    film PREFIX EVERY COUNT a picture every EVERY frames, COUNT times
    vars NAME...            IntyBASIC variables
    trace N EVERY NAME...   them every EVERY frames, N times
    peek ADDR N             N words from ADDR (hex)
    poke ADDR VAL...        write them
    stack                   how much of IntyBASIC's 24-word stack has been used at most: at the start its
                            free words are marked $BEEF, and the highest one overwritten since is reported
Environment: JZINTV_DIR, JZINTV, FUJINET (host:port, default 127.0.0.1:9980), ECS (1: attach one), DUMPDIR.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_intv  # noqa: E402

JZ = os.environ.get("JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))
JZINTV = os.environ.get("JZINTV", os.path.join(JZ, "bin", "linux", "jzintv"))
FUJINET = os.environ.get("FUJINET", "127.0.0.1:9980")
DUMPDIR = os.environ.get("DUMPDIR")
HOOK = 0x8FF0
ECS_HOOK = 0x8FF2
KEYPAD = {"1": 0x81, "2": 0x41, "3": 0x21, "4": 0x82, "5": 0x42, "6": 0x22, "7": 0x84, "8": 0x44,
          "9": 0x24, "C": 0x88, "0": 0x48, "E": 0x28}
BUTTON = {"T": 0xA0, "L": 0x60, "R": 0xC0}
DISC = {"N": 0x04, "E": 0x02, "S": 0x01, "W": 0x08}

class Jz:
    def __init__(self, rom, sym):
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        ecs = ["-s1", "-E", os.path.join(JZ, "rom", "ecs.bin")] if os.environ.get("ECS") == "1" else ["-s0"]
        self.p = subprocess.Popen(
            [JZINTV, "-d", "-e", os.path.join(JZ, "rom", "exec.bin"), "-g", os.path.join(JZ, "rom", "grom.bin")]
            + ecs + ["--fujinet=" + FUJINET, "-r1", rom],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, bufsize=0)
        self.buf = b""
        self.read_prompt()
        self.cmd("l " + sym)
        self.syms = {}
        for ln in open(sym):
            a, _, n = ln.strip().partition(" ")
            if n and re.match(r"^[0-9A-Fa-f]+$", a):
                self.syms[n] = int(a, 16)
        self.isr = self.syms["label_FRAME_ISR"]
        self.dump_pc = self.syms.get("label_DUMP_HOOK")
        self.cmd("b label_FRAME_ISR")
        self.frames = 0
        self.dumps = 0
        self.pc = None
        self.sp = self.low_sp = 0           # R6 at the breakpoints, and the deepest seen (the stack grows up)

    def read_prompt(self):
        while True:
            i = self.buf.find(b"\n> ")
            if i < 0 and self.buf.startswith(b"> "):
                self.buf = self.buf[2:]
                return ""
            if i >= 0:
                out, self.buf = self.buf[:i + 1], self.buf[i + 3:]
                return out.decode("latin1")
            ch = os.read(self.p.stdout.fileno(), 65536)
            if not ch:
                raise SystemExit("jzIntv exited:\n" + self.buf.decode("latin1")[-2000:])
            self.buf += ch

    def cmd(self, c):
        self.p.stdin.write((c + "\n").encode())
        self.p.stdin.flush()
        return self.read_prompt()

    def run(self, n=1):
        """n frames; the PC where it stopped last in self.pc (a breakpoint's)."""
        for _ in range(n):
            out = self.cmd("r")
            m = re.findall(r"Hit breakpoint at \$([0-9A-F]{4})", out)
            self.pc = int(m[-1], 16) if m else None
            regs = re.findall(r"^ ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) "
                              r"([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ", out, re.M)
            if regs:
                self.sp = int(regs[-1][6], 16)
                self.low_sp = max(self.low_sp, self.sp)
            if self.pc != self.dump_pc:
                self.frames += 1

    def mem(self, addr, n):
        out = self.cmd("m %x %x" % (addr, n))
        words = {}
        for ln in out.splitlines():
            m = re.match(r"^([0-9A-F]{4}):((?:\s+[0-9A-F]{4}\*?){1,8})", ln)
            if m:
                a = int(m.group(1), 16)
                for i, t in enumerate(re.findall(r"[0-9A-F]{4}", m.group(2))):
                    words[a + i] = int(t, 16)
        return [words.get(addr + i, 0) for i in range(n)]

    def poke(self, addr, val):
        self.cmd("e %x %x" % (addr, val))

    def addr(self, name):
        n = name.upper()
        for s in ("var_" + n, "var_&" + n, "array_" + n):
            if s in self.syms:
                return self.syms[s]
        raise SystemExit("no variable " + name)

    def var(self, name):
        return self.mem(self.addr(name), 1)[0]

    def dump(self):
        """The screen as render_intv.py's dump: BACKTAB, GRAM, the mode."""
        return render_intv.pack(self.mem(0x200, 240), [w & 0xFF for w in self.mem(0x3800, 512)],
                                self.var("scr_mode") & 1)

    def screen_text(self):
        return render_intv.text(self.dump())

    def render(self, path):
        d = self.dump()
        render_intv.render(d, path)
        if DUMPDIR:                                     # the dumps too, numbered (tools/capture.sh)
            self.dumps += 1
            with open(os.path.join(DUMPDIR, "scr%03d.bin" % self.dumps), "wb") as f:
                f.write(d)
        return d

    def press(self, byte, frames=4):
        self.poke(HOOK, 0xA5)
        self.poke(HOOK + 1, byte)
        self.run(frames)
        self.poke(HOOK + 1, 0)
        self.run(3)

    def ecs(self, text):
        """Typed on the ECS: each key at ECS_HOOK, which the frame handler takes as the scanner's."""
        if not self.var("ecs_present") & 0xFF:
            raise SystemExit("ecs: the game found no ECS (ECS=1)")
        i = 0
        while i < len(text):
            ch = text[i]
            if ch == "\\" and i + 1 < len(text):
                i += 1
                k = {"n": 0x0A, "e": 27, "b": 0x1C, "r": 0x1D, "u": 0x1E, "d": 0x1F}[text[i]]
            else:
                k = ord(ch.lower()) if ch.isalpha() else ord(ch)
            i += 1
            self.poke(ECS_HOOK, k)
            self.run(6)


def show(jz, name=None):
    if name:
        print("--- %s (frame %d) ---" % (name, jz.frames))
    for r, t in enumerate(jz.screen_text()):
        print("%2d|%s|" % (r, t))


def main():
    rom, scen, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(outdir, exist_ok=True)
    jz = Jz(rom, rom[:-4] + ".sym")
    lines = open(scen).read().splitlines() if scen != "-" else sys.stdin.read().splitlines()
    jz.run(1)                                           # into the frame handler: past the program's start
    jz.stack = jz.syms["STACK"]                         # IntyBASIC's 24 words: mark what isn't in use yet
    for a in range(jz.sp + 1, jz.stack + 24):
        jz.poke(a, 0xBEEF)
    for ln in lines:
        ln = ln.split("#", 1)[0].strip()
        if not ln:
            continue
        op, _, arg = ln.partition(" ")
        if op == "wait":
            jz.run(int(arg))
        elif op == "snap":
            jz.render(os.path.join(outdir, arg + ".png"))
            show(jz, arg)
        elif op == "text":
            show(jz)
        elif op == "key":
            jz.press(KEYPAD[arg.upper()])
        elif op == "btn":
            jz.press(BUTTON[arg.upper()])
        elif op == "disc":
            d, _, n = arg.partition(" ")
            jz.press(DISC[d.upper()], int(n or 4))
        elif op == "hold":
            b, n = arg.split()
            jz.press(int(b, 16), int(n))
        elif op == "ecs":
            jz.ecs(arg)
        elif op in ("until", "untilnot"):
            parts = arg.rsplit(" ", 1)
            mx, text = 3600, arg
            if len(parts) == 2 and parts[1].isdigit():
                text, mx = parts[0], int(parts[1])
            n = 0
            while n < mx:
                jz.run(10)
                n += 10
                if any(text in t for t in jz.screen_text()) == (op == "until"):
                    break
            print("%s %r: %d frames (%s)" % (op, text, n, "ok" if n < mx else "TIMEOUT"))
        elif op == "untilvar":
            parts = arg.split()
            mx = int(parts[2]) if len(parts) > 2 else 3600
            want, n = int(parts[1], 0), 0
            while n < mx and (jz.var(parts[0]) & 0xFF) != want:
                jz.run(10)
                n += 10
            print("untilvar %s=%d: %d frames (%s)" % (parts[0], want, n, "ok" if n < mx else "TIMEOUT"))
        elif op == "dumps":
            parts = arg.split()
            pre, count = parts[0], int(parts[1])
            mx = int(parts[2]) if len(parts) > 2 else 36000
            if jz.dump_pc is None:
                raise SystemExit("dumps: no dump_hook (the autoplay build has one)")
            jz.cmd("b label_DUMP_HOOK")
            got, start = 0, jz.frames
            while got < count and jz.frames - start < mx:
                jz.run(1)
                if jz.pc == jz.dump_pc:
                    name = "%s_%03d" % (pre, got)
                    jz.render(os.path.join(outdir, name + ".png"))
                    show(jz, name)
                    got += 1
            jz.cmd("n label_DUMP_HOOK")
            print("dumps: %d in %d frames" % (got, jz.frames - start))
        elif op == "film":
            pre, every, count = arg.split()
            for i in range(int(count)):
                jz.run(int(every))
                jz.render(os.path.join(outdir, "%s_%03d.png" % (pre, i)))
                print("film %s_%03d frame %d" % (pre, i, jz.frames))
        elif op == "trace":
            # trace N EVERY NAME...: the variables every EVERY frames, N times
            parts = arg.split()
            for _ in range(int(parts[0])):
                jz.run(int(parts[1]))
                print("f%d" % jz.frames, " ".join("%s=%d" % (v, jz.var(v) & 0xFFFF) for v in parts[2:]))
        elif op == "stack":
            words = jz.mem(jz.stack, 24)
            used = max([i for i, w in enumerate(words) if w != 0xBEEF] or [-1])
            print("stack: $%04X-$%04X used at most (of $%04X-$%04X); R6 at the frame handler at most $%04X"
                  % (jz.stack, jz.stack + used, jz.stack, jz.stack + 23, jz.low_sp))
        elif op == "vars":
            print("vars", " ".join("%s=%d" % (v, jz.var(v) & 0xFFFF) for v in arg.split()))
        elif op == "peek":
            a, _, n = arg.partition(" ")
            print(arg, " ".join("%04X" % w for w in jz.mem(int(a, 16), int(n or 1))))
        elif op == "poke":
            a, *vals = arg.split()
            for k, v in enumerate(vals):
                jz.poke(int(a, 16) + k, int(v, 0))
        else:
            raise SystemExit("unknown: " + ln)
    jz.p.stdin.write(b"q\n")
    jz.p.stdin.flush()


if __name__ == "__main__":
    main()
