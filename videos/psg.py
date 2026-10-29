#!/usr/bin/env python3
"""psg.py -- play song.py on jzIntv's own AY-3-8914 and keep what it sounds like: build/music.wav.

An idle cartridge (psgidle.bas) runs in jzIntv's debugger with a breakpoint on its frame handler, so each 'r' is
one frame, at the console's own pace (-r1: with SDL's dummy audio, -r0 stalls once its sound buffers fill). Between frames the registers that changed are written ($1F0 + n), as a sound driver in the frame
handler would write them. jzIntv's -F file starts at the first sound, so a click at frame CLICK marks the time;
the music starts at frame PRE, (PRE - CLICK) frames after the click's onset.

usage: psg.py [--bars A-B]   build/music.wav: 48 kHz stereo, -16 LUFS, LENGTH s (--bars: only those bars,
                             to build/music_A-B.wav, for listening)
       psg.py --finish         build/music.wav again from the last recording (build/music_raw.wav)
The level leaves room for AAC: square waves come out of it about 2 dB over their peak.
Needs build/psgidle.rom and .sym (the Makefile builds them) and jzIntv (JZINTV_DIR).
"""
import os
import re
import subprocess
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import song  # noqa: E402

BUILD = os.path.join(HERE, "build")
JZ = os.environ.get("JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))
JZINTV = os.environ.get("JZINTV", os.path.join(JZ, "bin", "linux", "jzintv"))
RATE = 48000
CPU = 3579545 / 4                   # CP-1610 cycles a second
FRAME = 14934                       # cycles a frame
CLICK, PRE = 2, 60
LENGTH = 120.0
LOUD = "I=-16:TP=-3:LRA=11"         # loudnorm's target


class Jz:
    def __init__(self, rom, sym, wav):
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
        self.p = subprocess.Popen(
            [JZINTV, "-d", "-e", os.path.join(JZ, "rom", "exec.bin"), "-g", os.path.join(JZ, "rom", "grom.bin"),
             "-r1", "-a%d" % RATE, "-F" + wav, rom],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, bufsize=0, cwd=BUILD)
        self.buf = b""
        self.prompt()
        self.cmd("l " + sym)
        self.cmd("b label_TICK")

    def prompt(self):
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
        return self.prompt()

    def frame(self):
        out = self.cmd("r")
        m = re.findall(r"Hit breakpoint at \$([0-9A-F]{4})", out)
        if not m:
            raise SystemExit("no breakpoint:\n" + out)

    def quit(self):
        self.p.stdin.write(b"q\n")
        self.p.stdin.flush()
        self.p.wait(timeout=30)


def play(regs, raw):
    """Write regs (frames x 15, see song.registers) to the PSG a frame at a time, from frame PRE."""
    jz = Jz(os.path.join(BUILD, "psgidle.rom"), os.path.join(BUILD, "psgidle.sym"), raw)
    jz.frame()
    cur = [None] * 14
    click = {CLICK: {0: 0x40, 4: 0, 8: 0x3E, 11: 15}, CLICK + 2: {11: 0}}
    for k in range(PRE + len(regs) + 4):
        writes = click.get(k, {})
        if PRE <= k < PRE + len(regs):
            row = regs[k - PRE]
            writes = {}
            for r in range(14):
                v = int(row[r])
                if r == 10:
                    if row[14] or cur[10] is None:
                        writes[r] = v
                elif v != cur[r]:
                    writes[r] = v
        elif k == PRE + len(regs):
            writes = {11: 0, 12: 0, 13: 0}
        for r, v in writes.items():
            jz.cmd("e %x %x" % (0x1F0 + r, v))
            cur[r] = v
        jz.frame()
        if k % 1200 == 0:
            print("  frame %d of %d" % (k, PRE + len(regs)), flush=True)
    jz.quit()


def finish(raw, out, seconds):
    sr, x = wavfile.read(raw)
    if sr != RATE:
        raise SystemExit("%s: %d Hz" % (raw, sr))
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    x -= x[:64].mean()
    onset = int(np.argmax(np.abs(x) > 200))
    start = onset + int(round((PRE - CLICK) * FRAME / CPU * RATE))
    n = int(round(seconds * RATE))
    y = x[start:start + n]
    if len(y) < n:
        y = np.concatenate([y, np.zeros(n - len(y))])
    y = sosfilt(butter(2, 20, "highpass", fs=RATE, output="sos"), y)         # the PSG's output is all positive
    y = sosfilt(butter(1, 12000, "lowpass", fs=RATE, output="sos"), y)       # an RF modulator's softening
    fade = int(0.05 * RATE)
    y[-fade:] *= np.linspace(1, 0, fade)
    y /= max(1e-9, np.abs(y).max())
    tmp = out + ".pre.wav"
    wavfile.write(tmp, RATE, (np.stack([y, y], axis=1) * 0.5 * 32767).astype(np.int16))
    meas = subprocess.run(["ffmpeg", "-hide_banner", "-i", tmp, "-af", "loudnorm=%s:print_format=json" % LOUD,
                           "-f", "null", "-"], capture_output=True, text=True).stderr
    j = dict(re.findall(r'"(input_i|input_tp|input_lra|input_thresh|target_offset)" : "([-0-9.inf]+)"', meas))
    af = ("loudnorm=%s:measured_I=%s:measured_TP=%s:measured_LRA=%s:measured_thresh=%s:offset=%s:linear=true"
          % (LOUD, j["input_i"], j["input_tp"], j["input_lra"], j["input_thresh"], j["target_offset"]))
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", tmp, "-af", af, "-ar", str(RATE), "-c:a", "pcm_s16le", out],
                   check=True)
    os.remove(tmp)
    print("onset at sample %d; music from %d; wrote %s (%.2f s)" % (onset, start, out, seconds))


def main():
    regs = song.registers()
    raw = os.path.join(BUILD, "music_raw.wav")
    out = os.path.join(BUILD, "music.wav")
    seconds = LENGTH
    if "--bars" in sys.argv:
        a, b = map(int, sys.argv[sys.argv.index("--bars") + 1].split("-"))
        regs = regs[song.fr(a):song.fr(b + 1)]
        raw = os.path.join(BUILD, "music_raw_%d-%d.wav" % (a, b))
        out = os.path.join(BUILD, "music_%d-%d.wav" % (a, b))
        seconds = len(regs) / song.FPS
    if "--finish" not in sys.argv:
        if os.path.exists(raw):
            os.remove(raw)
        play(regs, os.path.basename(raw))
    finish(raw, out, seconds)


if __name__ == "__main__":
    main()
