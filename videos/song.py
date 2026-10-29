#!/usr/bin/env python3
"""song.py -- the teaser's tune: an original D-minor hacker piece for the Intellivision's AY-3-8914, written as
what a sound driver would write to the PSG once a frame. psg.py plays these frames on jzIntv's own PSG.

64 bars of 16 sixteenths, 7 frames a sixteenth at the NTSC frame rate (59.92 Hz): 128.4 BPM, 119.6 s. The
channels: A the bass (a tone, or the hardware envelope as a sawtooth: the AY's "buzz" bass), B arpeggios and the
drums (noise), C the lead. teaser.py cuts on the bars: BAR_S and at() are its clock.

    bars  1-4   boot: a buzz drone, static, the SYN / SYN-ACK / ACK chirps
          5-8   title: the hit, then the bass
          9-16  premise, jack-in: bass, arps, drums
         17-32  play: the theme (loop A twice), the riff (loop B twice)
         33-40  hivejack: half time, the buzz bass, an envelope dive
         41-48  the ECS: typing clicks, a slow arp; a build
         49-60  finale: the theme an octave up, the riff
         61-64  outro: the last hit, ringing out; the chirps again

usage: song.py            a summary of the registers
"""
import math
import random

import numpy as np

CLOCK = 3579545 / 32            # the PSG's clock in jzIntv: tone f = CLOCK / P, noise CLOCK / N, envelope
FPS = 3579545 / 4 / 14934       # steps CLOCK / E (16 to a ramp); NTSC frames a second
STEP = 7                        # frames a sixteenth
BAR = 16 * STEP                 # frames a bar
BARS = 64
BAR_S = BAR / FPS
TAIL = 60                       # frames after the last bar, to ring out
N = BARS * BAR + TAIL

A, B, C = 0, 1, 2
ENV_FULL, ENV_HALF, ENV_QUARTER = 0x30, 0x20, 0x10     # the 8914's envelope select in R11-R13
SAW = 0x08                                              # R10: CONT, sawtooth down, repeating


def at(bar, six=0):
    """Seconds from the start to sixteenth `six` of bar `bar` (from 1)."""
    return fr(bar, six) / FPS


def fr(bar, six=0):
    return (bar - 1) * BAR + six * STEP


# ---------------------------------------------------------------------------
# Pitches
# ---------------------------------------------------------------------------
NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def midi(name):
    """'C#5' -> 73 (C4 = 60); flats as 'Bb4'."""
    m = NOTE[name[0]]
    rest = name[1:]
    while rest and rest[0] in "#b":
        m += 1 if rest[0] == "#" else -1
        rest = rest[1:]
    return m + 12 * (int(rest) + 1)


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def period(f):
    return max(1, min(4095, int(round(CLOCK / f))))


def eperiod(f):
    return max(1, min(65535, int(round(CLOCK / (16 * f)))))


CHORD = {"Dm": "D F A", "Bb": "Bb D F", "Gm": "G Bb D", "A": "A C# E", "Eb": "Eb G Bb", "C#dim": "C# E G"}
LOOP_A = ["Dm", "Bb", "Gm", "A"]
LOOP_B = ["Dm", "Eb", "Dm", "C#dim"]


def root(ch, octave):
    return midi(CHORD[ch].split()[0] + str(octave))


def tones(ch, octave, n=3):
    """The chord's notes rising from its root in `octave`, n of them (repeating an octave up)."""
    names = CHORD[ch].split()
    out, last = [], -1
    for i in range(n):
        m = midi(names[i % len(names)] + str(octave)) + 12 * (i // len(names))
        while m <= last:
            m += 12
        out.append(m)
        last = m
    return out


# ---------------------------------------------------------------------------
# The registers, a frame at a time: what each channel plays, then packed
# ---------------------------------------------------------------------------
per = np.zeros((N, 3), dtype=np.int32)          # tone period
vol = np.zeros((N, 3), dtype=np.int32)          # 0-15, or an ENV_ select
ton = np.zeros((N, 3), dtype=bool)
noi = np.zeros((N, 3), dtype=bool)
nper = np.full(N, 1, dtype=np.int32)
eper = np.full(N, 1, dtype=np.int32)
eshape = np.full(N, SAW, dtype=np.int32)
retrig = np.zeros(N, dtype=bool)


def put(ch, f, p=0, v=0, tone=True, noise=False, env=None):
    """One channel's frame. v: a volume, clamped to 0-15; env: an ENV_ select instead (buzz())."""
    v = env if env is not None else max(0, min(15, int(v)))
    if 0 <= f < N:
        per[f, ch], vol[f, ch], ton[f, ch], noi[f, ch] = p, v, tone, noise


def note(ch, f0, n, m, vols, rel=(), vib=0.0, vib_after=12, slide=None):
    """MIDI m for n frames: vols frame by frame (the last held), then the release. slide: the MIDI it glides to
    over the n frames. vib: vibrato depth (a fraction of the frequency) at 6 Hz after vib_after frames."""
    for i in range(n + len(rel)):
        if i < n:
            v = vols[min(i, len(vols) - 1)]
        else:
            v = rel[i - n]
        mm = m if slide is None else m + (slide - m) * min(1.0, i / max(1, n - 1))
        f = hz(mm)
        if vib and i >= vib_after:
            f *= 1 + vib * math.sin(2 * math.pi * 6 * (i - vib_after) / FPS)
        put(ch, f0 + i, period(f), v)


def arp(ch, f0, n, notes, vols, rate=1):
    """The notes in turn, one every `rate` frames, for n frames, with vols frame by frame (the last held)."""
    for i in range(n):
        put(ch, f0 + i, period(hz(notes[(i // rate) % len(notes)])), vols[min(i, len(vols) - 1)])


def buzz(f0, n, m, sel=ENV_FULL, slide=None, start=False):
    """Channel A as the hardware envelope's sawtooth at MIDI m (the tone off): the buzz bass. start: restart the
    envelope on the first frame."""
    for i in range(n):
        f = f0 + i
        if not 0 <= f < N:
            continue
        mm = m if slide is None else m + (slide - m) * min(1.0, i / max(1, n - 1))
        put(A, f, 0, tone=False, env=sel)
        eper[f] = eperiod(hz(mm))
        eshape[f] = SAW
    if start and 0 <= f0 < N:
        retrig[f0] = True


def noise(ch, f0, nps, vols, tone_hz=None):
    """Noise on ch: nps and vols frame by frame (vols' length is the length). tone_hz: a tone mixed in on the
    first frame (a snare's crack)."""
    for i, v in enumerate(vols):
        f = f0 + i
        if not 0 <= f < N:
            continue
        t = tone_hz is not None and i < 2
        put(ch, f, period(tone_hz) if t else 0, v, tone=t, noise=True)
        nper[f] = nps[min(i, len(nps) - 1)]


def hat(ch, f0, v=10):
    noise(ch, f0, [1], [v, v * 5 // 10, v * 3 // 10])


def ohat(ch, f0, v=9):
    noise(ch, f0, [1], [max(0, v - i) for i in range(v)])


def snare(ch, f0, v=15):
    noise(ch, f0, [6, 7, 8, 9, 10], [max(0, v - k) for k in (0, 2, 4, 5, 7, 8, 10, 11, 12, 13)], tone_hz=210)


def crash(ch, f0, n=BAR, v=14):
    noise(ch, f0, [2, 3], [int(round(v * (1 - i / n) ** 1.5)) for i in range(n)])


def click(ch, f0, v=11, np_=2):
    noise(ch, f0, [np_], [v, v // 2, 1])


def kick(ch, f0):
    for i, (f, v) in enumerate(zip([190, 130, 92, 68, 54, 46], [15, 15, 14, 12, 9, 5])):
        put(ch, f0 + i, period(f), v)


def mel(ch, bar, line, transpose=0, vols=(11, 13, 13, 12, 12, 11), rel=(8, 5, 3), vib=0.005, gap=1, six=0):
    """A line of notes from sixteenth `six` of `bar`: 'D5:3 F5:3 r:2 ...', lengths in sixteenths."""
    pos = six
    for tok in line.split():
        name, _, ln = tok.partition(":")
        ln = int(ln or 1)
        if name != "r":
            note(ch, fr(bar) + pos * STEP, ln * STEP - gap, midi(name) + transpose, vols, rel, vib)
        pos += ln
    return pos


# ---------------------------------------------------------------------------
# Parts
# ---------------------------------------------------------------------------
GALLOP = [(0, 2, 0), (2, 1, 0), (3, 1, 12), (4, 2, 0), (6, 1, 0), (7, 1, 12), (8, 2, 0), (10, 1, 0),
          (11, 1, 12), (12, 2, 0), (14, 1, 12), (15, 1, 0)]       # (sixteenth, length, octave up)
PLUCK = (14, 13, 12, 11, 10)


def bass_bar(bar, ch_name, octave=2, kicks=(0, 8)):
    r = root(ch_name, octave)
    for s, ln, up in GALLOP:
        note(A, fr(bar, s), ln * STEP - 1, r + up, PLUCK, (6, 3))
    for s in kicks:
        kick(A, fr(bar, s))


def fast_arp_bar(ch, bar, ch_name, octave=4, loud=9):
    notes = tones(ch_name, octave)
    vols = [loud, loud - 1, loud - 1, loud - 2, loud - 2, loud - 2, loud - 2] + [loud - 3] * 7
    for e in range(8):                                 # retriggered every eighth
        arp(ch, fr(bar, e * 2), 2 * STEP, notes, vols)


def drums_bar(bar, snares=(4, 12), hats=(2, 6, 10, 14), fill=False, hat_v=9):
    for s in hats:
        hat(B, fr(bar, s), hat_v)
    for s in snares:
        snare(B, fr(bar, s))
    if fill:
        for s in (12, 13, 14, 15):
            snare(B, fr(bar, s), 11 + (s - 12))


THEME = [   # over loop A, twice
    "D5:3 F5:3 A5:2 G5:2 F5:2 E5:2 F5:2",
    "D5:6 r:2 Bb4:2 D5:2 F5:4",
    "G5:3 F5:3 D5:2 Bb4:3 D5:3 G5:2",
    "A5:6 G5:2 E5:2 C#5:2 A4:4",
    "D5:3 F5:3 A5:2 D6:4 C6:2 A5:2",
    "Bb5:6 A5:2 F5:4 D5:4",
    "G5:2 A5:2 Bb5:4 A5:2 G5:2 F5:2 E5:2",
    "C#5:3 E5:3 A5:6 r:4",
]
RIFF = [    # over loop B, twice; the last bar rises
    "D5:1 r:1 D5:1 r:1 F5:2 D5:2 A5:2 G#5:2 A5:2 r:2",
    "G5:2 Eb5:2 Bb4:2 Eb5:2 G5:4 F5:2 Eb5:2",
    "D5:1 r:1 D5:1 r:1 F5:2 A5:2 D6:4 C#6:4",
    "Bb5:4 G5:4 E5:4 C#5:4",
    "D5:1 r:1 D5:1 r:1 F5:2 D5:2 A5:2 G#5:2 A5:2 r:2",
    "G5:2 Eb5:2 Bb4:2 Eb5:2 G5:4 F5:2 Eb5:2",
    "D5:1 r:1 D5:1 r:1 F5:2 A5:2 D6:4 C#6:4",
    "E5:2 G5:2 Bb5:2 C#6:2 E6:8",
]
JACK = [    # the hivejack, half time over loop B twice
    "D6:4 A5:4 G#5:4 D5:4",
    "Eb5:8 G5:4 Bb5:4",
    "A5:6 G#5:2 A5:4 D5:4",
    "E5:4 G5:4 Bb5:4 C#6:4",
    "D6:8 C#6:4 A5:4",
    "Bb5:8 G5:4 Eb5:4",
    "D5:4 F5:4 A5:4 G#5:4",
    "C#6:4 Bb5:4 G5:4 E5:4",
]
STACCATO = (12, 13, 11, 9, 8)


def chirp(bar, six, names, v=11, each=3):
    f = fr(bar, six)
    for k, nm in enumerate(names):
        note(C, f + k * each, each if k < len(names) - 1 else each + 2, midi(nm), [v], (v // 2, 2))


def boot(rng):
    for bar in range(1, 5):
        for s in range(16):                             # the drone: a thump on beats 1 and 3
            sel = ENV_FULL if s in (0, 1, 8) else ENV_HALF if s in (2, 9) else ENV_QUARTER
            buzz(fr(bar, s), STEP, midi("D1"), sel, start=(bar == 1 and s == 0))
        for i in range(BAR):                            # static, swelling
            f = fr(bar) + i
            level = (bar - 1 + i / BAR) / 4
            v = max(0, min(7, int(level * 7 + rng.choice((-1, 0, 0, 1)))))
            put(B, f, 0, v, tone=False, noise=True)
            nper[f] = rng.randint(12, 31)
        for s in range(16):                             # data blips
            if s in (4, 5, 6) or rng.random() > 0.3:
                continue
            nm = rng.choice(["D6", "F6", "G6", "A6", "C7"])
            note(C, fr(bar, s), 2, midi(nm), [6], (3,))
    chirp(1, 4, ["D6", "A6"])                           # > SYN
    chirp(2, 4, ["A6", "D6", "A6"])                     # < SYN-ACK
    chirp(3, 4, ["A6", "D7"])                           # > ACK
    arp(C, fr(4), 4 * STEP, [midi(n) for n in ("D6", "F6", "A6", "D7")], [10, 10, 9, 9, 8], rate=2)
    for s in range(8, 16):                              # a snare roll into the title
        snare(B, fr(4, s), 8 + (s - 8))


def title():
    buzz(fr(5), 8 * STEP, midi("D1"), ENV_FULL, start=True)
    buzz(fr(5, 8), 8 * STEP, midi("D1"), ENV_HALF)
    crash(B, fr(5), BAR)
    arp(C, fr(5), 2 * BAR, tones("Dm", 5, 4), [max(0, 14 - i // 10) for i in range(2 * BAR)])
    for bar, ch in zip(range(6, 9), LOOP_A[1:]):
        bass_bar(bar, ch, kicks=())
        drums_bar(bar, snares=(), fill=(bar == 8))
    for bar, ch in zip(range(7, 9), LOOP_A[2:]):
        fast_arp_bar(C, bar, ch, 4, 9)


def premise():
    for k, bar in enumerate(range(9, 17)):
        ch = LOOP_A[k % 4]
        bass_bar(bar, ch, kicks=(0, 8))
        fast_arp_bar(C, bar, ch, 4, 10)
        if bar < 13:
            drums_bar(bar, snares=())
        else:
            drums_bar(bar, fill=(bar == 16))


def play(bar0, transpose=0, riff=RIFF):
    """The theme over loop A twice, then the riff over loop B: 8 + len(riff) bars from bar0."""
    last = 8 + len(riff) - 1
    for k in range(last + 1):
        bar = bar0 + k
        ch = (LOOP_A if k < 8 else LOOP_B)[k % 4]
        bass_bar(bar, ch, kicks=(0, 8) if k < 8 else (0, 8, 10))
        fast_arp_bar(B, bar, ch, 4, 8)
        drums_bar(bar, hat_v=8, fill=(k == last))
        if k < 8:
            mel(C, bar, THEME[k], transpose)
        else:
            line = riff[k - 8]
            mel(C, bar, line, 0, vols=STACCATO if "r:1" in line else (11, 13, 13, 12, 12, 11))


def hivejack():
    buzz(fr(33), BAR, midi("D2"), ENV_FULL, slide=midi("D0"), start=True)     # the dive
    crash(B, fr(33), BAR)
    riff = [("D", 1), ("Eb", 1), ("D", 1), ("C#", 1)]
    for k, bar in enumerate(range(33, 41)):
        nm, o = riff[k % 4]
        r = midi(nm + str(o))
        if bar > 33:
            for s, ln, up in [(0, 6, 0), (6, 2, 12), (8, 4, 0), (12, 2, 12), (14, 2, 7)]:
                buzz(fr(bar, s), ln * STEP - 1, r + up, ENV_FULL)
            kick(B, fr(bar, 0))
            snare(B, fr(bar, 8))
            for s in (4, 12, 14):
                hat(B, fr(bar, s), 8)
        mel(C, bar, JACK[k], 0, vib=0.007)
    for s in range(8, 16):
        snare(B, fr(40, s), 9 + (s - 8) // 2)


def ecs(rng):
    chords = ["Dm", "Dm", "Bb", "Bb", "Gm", "Gm", "A", "A"]
    for k, bar in enumerate(range(41, 49)):
        ch = chords[k]
        if bar < 47:
            for s in (0, 4, 8, 12):
                note(A, fr(bar, s), 3 * STEP, root(ch, 2), (14, 13, 12, 11), (7, 4))
            notes = tones(ch, 5, 4)
            seq = notes + notes[-2:0:-1]                # up and down
            for s in range(16):
                note(C, fr(bar, s), STEP - 1, seq[s % len(seq)], (12, 11, 9, 8, 7, 6), ())
            for s in range(16):                         # someone typing
                if rng.random() < (0.35 if s % 2 else 0.5):
                    click(B, fr(bar, s) + rng.randint(0, 3), rng.choice((11, 12, 13)))
            if bar in (44, 46):
                noise(B, fr(bar, 14), [5], [13, 9, 6, 3])   # RTN
        else:
            bass_bar(bar, ch, kicks=(0, 8) if bar == 48 else ())
            notes = tones(ch, 5 if bar == 47 else 6, 4)
            for s in range(16):
                note(C, fr(bar, s), STEP - 1, notes[s % 4], (11, 10, 8, 6), ())
            step = 2 if bar == 47 else 1
            for s in range(0, 16, step):
                snare(B, fr(bar, s), 8 + (s // 2 if bar == 47 else 4 + s // 3))


def finale():
    play(49, transpose=12, riff=RIFF[4:])       # 49-56 the theme an octave up, 57-60 the riff rising out


def outro():
    buzz(fr(61), BAR, midi("D1"), ENV_FULL, start=True)
    buzz(fr(62), BAR, midi("D1"), ENV_HALF)
    buzz(fr(63), BAR, midi("D1"), ENV_QUARTER)
    crash(B, fr(61), 2 * BAR, 14)
    note(C, fr(61), 2 * BAR, midi("D5"), [max(4, 13 - i // 28) for i in range(2 * BAR)], (3, 2, 1), vib=0.006,
         vib_after=20)
    notes = tones("Dm", 4, 4)
    for s in range(16):
        v = max(0, 8 - s // 2)
        note(B, fr(63, s), STEP - 1, notes[s % 4], (v, max(0, v - 2)), ())
    chirp(63, 4, ["D6", "A6"], v=7)
    chirp(64, 4, ["A6", "D7"], v=5)


def build():
    rng = random.Random(1964)
    boot(rng)
    title()
    premise()
    play(17)
    hivejack()
    ecs(rng)
    finale()
    outro()


def registers():
    """N x 15: R0-R13 as jzIntv numbers them ($1F0 + n), then 1 where R10 is written (the envelope restarts)."""
    build()
    out = np.zeros((N, 15), dtype=np.int32)
    out[:, 0:3] = per & 0xFF
    out[:, 4:7] = (per >> 8) & 0x0F
    out[:, 3] = eper & 0xFF
    out[:, 7] = (eper >> 8) & 0xFF
    mix = np.zeros(N, dtype=np.int32)
    for ch in range(3):
        mix |= (~ton[:, ch]).astype(np.int32) << ch
        mix |= (~noi[:, ch]).astype(np.int32) << (3 + ch)
    out[:, 8] = mix
    out[:, 9] = nper & 0x1F
    out[:, 10] = eshape & 0x0F
    out[:, 11:14] = vol & 0x3F
    out[:, 14] = retrig
    return out


if __name__ == "__main__":
    r = registers()
    print("%d frames (%d bars + %d), %.2f s; bar %.4f s" % (N, BARS, TAIL, N / FPS, BAR_S))
    print("frames sounding: A %d, B %d, C %d; envelope restarts %d"
          % tuple([int((r[:, 11 + c] > 0).sum()) for c in range(3)] + [int(r[:, 14].sum())]))
