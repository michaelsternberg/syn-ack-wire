#!/usr/bin/env python3
"""A small QR code encoder (ISO/IEC 18004), standard library only, for the help pages (help_disk.py).

    qr.py TEXT        print the code as text, to look at

Versions 1-6 only (up to 41x41 modules, all a help page has room for, and none of them needs the version
information blocks). Alphanumeric mode when the text allows it (digits, upper case, space and $%*+-./:),
else byte mode. encode() can force a mask, so a caller can pick among the eight for its own reasons.
"""
import sys

ECC = "LMQH"
FORMAT_BITS = {"L": 1, "M": 0, "Q": 3, "H": 2}
# Per version 1-6 and level L, M, Q, H: (error correction codewords per block, blocks).
BLOCKS = {
    1: ((7, 1), (10, 1), (13, 1), (17, 1)),
    2: ((10, 1), (16, 1), (22, 1), (28, 1)),
    3: ((15, 1), (26, 1), (18, 2), (22, 2)),
    4: ((20, 1), (18, 2), (26, 2), (16, 4)),
    5: ((26, 1), (24, 2), (18, 4), (22, 4)),
    6: ((18, 2), (16, 4), (24, 4), (28, 4)),
}
ALIGN = {1: (), 2: (6, 18), 3: (6, 22), 4: (6, 26), 5: (6, 30), 6: (6, 34)}
ALNUM = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:"

# GF(256) with the QR polynomial x^8 + x^4 + x^3 + x^2 + 1
EXP, LOG = [0] * 512, [0] * 256
_x = 1
for _i in range(255):
    EXP[_i], LOG[_x] = _x, _i
    _x <<= 1
    if _x & 0x100:
        _x ^= 0x11D
for _i in range(255, 512):
    EXP[_i] = EXP[_i - 255]


def _mul(a, b):
    return 0 if a == 0 or b == 0 else EXP[LOG[a] + LOG[b]]


def _rs(data, n):
    """The n Reed-Solomon error correction codewords for `data`."""
    gen = [1]
    for i in range(n):          # (x - a^0)(x - a^1)...(x - a^(n-1))
        gen = [a ^ _mul(b, EXP[i]) for a, b in zip(gen + [0], [0] + gen)]
    rem = [0] * n
    for d in data:
        f = d ^ rem[0]
        rem = rem[1:] + [0]
        for i in range(n):
            rem[i] ^= _mul(gen[i + 1], f)
    return rem


def _data_codewords(version, level):
    ec, blocks = BLOCKS[version][ECC.index(level)]
    raw = (16 * version + 128) * version + 64
    if version >= 2:
        n = version // 7 + 2
        raw -= (25 * n - 10) * n - 55
    return raw // 8 - ec * blocks


def _bits(text, version):
    """The data bits: mode, count, data (count fields are the versions 1-9 sizes)."""
    out = []

    def put(v, n):
        out.extend((v >> i) & 1 for i in reversed(range(n)))
    if all(c in ALNUM for c in text):
        put(0b0010, 4)
        put(len(text), 9)
        for i in range(0, len(text) - 1, 2):
            put(45 * ALNUM.index(text[i]) + ALNUM.index(text[i + 1]), 11)
        if len(text) % 2:
            put(ALNUM.index(text[-1]), 6)
    else:
        data = text.encode("utf-8")
        put(0b0100, 4)
        put(len(data), 8)
        for b in data:
            put(b, 8)
    return out


def fits(text, version, level):
    return len(_bits(text, version)) <= 8 * _data_codewords(version, level)


def _codewords(text, version, level):
    cap = 8 * _data_codewords(version, level)
    bits = _bits(text, version)
    bits += [0] * min(4, cap - len(bits))           # terminator
    bits += [0] * (-len(bits) % 8)
    data = [int("".join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8)]
    data += [0xEC, 0x11] * ((cap // 8 - len(data)) // 2 + 1)
    data = data[:cap // 8]
    ec, nblocks = BLOCKS[version][ECC.index(level)]
    short = len(data) // nblocks                    # the first blocks may be one codeword shorter
    nlong = len(data) % nblocks
    blocks, k = [], 0
    for b in range(nblocks):
        n = short + (b >= nblocks - nlong)
        blocks.append(data[k:k + n])
        k += n
    out = [blk[i] for i in range(short + 1) for blk in blocks if i < len(blk)]
    eccs = [_rs(blk, ec) for blk in blocks]
    return out + [e[i] for i in range(ec) for e in eccs]


MASKS = (
    lambda r, c: (r + c) % 2 == 0,
    lambda r, c: r % 2 == 0,
    lambda r, c: c % 3 == 0,
    lambda r, c: (r + c) % 3 == 0,
    lambda r, c: (r // 2 + c // 3) % 2 == 0,
    lambda r, c: r * c % 2 + r * c % 3 == 0,
    lambda r, c: (r * c % 2 + r * c % 3) % 2 == 0,
    lambda r, c: ((r + c) % 2 + r * c % 3) % 2 == 0,
)


def _format(level, mask):
    d = FORMAT_BITS[level] << 3 | mask
    r = d << 10
    for i in range(14, 9, -1):
        if r >> i & 1:
            r ^= 0x537 << (i - 10)
    return (d << 10 | r) ^ 0x5412


def encode(text, level="M", version=None, mask=None):
    """The QR code for `text` as rows of 0/1 (1 = dark), in the smallest version 1-6 that holds it unless
    `version` is given, and with the lowest-penalty mask unless `mask` is. ValueError if it doesn't fit."""
    if version is None:
        version = next((v for v in BLOCKS if fits(text, v, level)), None)
        if version is None:
            raise ValueError(f"{len(text)} characters don't fit a version 1-6 QR code at level {level}")
    elif not fits(text, version, level):
        raise ValueError(f"doesn't fit version {version} at level {level}")
    size = 17 + 4 * version
    m = [[0] * size for _ in range(size)]
    fixed = [[False] * size for _ in range(size)]

    def set_(r, c, v):
        m[r][c] = v
        fixed[r][c] = True
    for r0, c0 in ((0, 0), (0, size - 7), (size - 7, 0)):          # finders and their separators
        for r in range(-1, 8):
            for c in range(-1, 8):
                if 0 <= r0 + r < size and 0 <= c0 + c < size:
                    ring = max(abs(r - 3), abs(c - 3))
                    set_(r0 + r, c0 + c, int(ring != 2 and ring != 4))
    for i in range(8, size - 8):                                    # timing
        set_(6, i, int(i % 2 == 0))
        set_(i, 6, int(i % 2 == 0))
    pos = ALIGN[version]
    for r in pos:
        for c in pos:
            if fixed[r][c]:
                continue                                            # under a finder
            for dr in range(-2, 3):
                for dc in range(-2, 3):
                    set_(r + dr, c + dc, int(max(abs(dr), abs(dc)) != 1))
    set_(size - 8, 8, 1)                                            # the dark module
    for r, c in _format_cells(size):                                # reserved for the format bits
        set_(r, c, 0)
    bits = [(w >> (7 - i)) & 1 for w in _codewords(text, version, level) for i in range(8)]
    k = 0
    upward = True
    c = size - 1
    while c > 0:                                                    # the zigzag, two columns at a time
        if c == 6:
            c -= 1
        for i in range(size):
            r = size - 1 - i if upward else i
            for cc in (c, c - 1):
                if not fixed[r][cc]:
                    m[r][cc] = bits[k] if k < len(bits) else 0
                    k += 1
        upward = not upward
        c -= 2

    def masked(n):
        out = [row[:] for row in m]
        for r in range(size):
            for c in range(size):
                if not fixed[r][c] and MASKS[n](r, c):
                    out[r][c] ^= 1
        for (r, c), i in zip(_format_cells(size), _FORMAT_ORDER):
            out[r][c] = _format(level, n) >> i & 1
        return out
    if mask is not None:
        return masked(mask)
    return min((masked(n) for n in range(8)), key=penalty)


_FORMAT_ORDER = list(range(15)) + list(range(15))


def _format_cells(size):
    """Where format bits 0-14 go, then their second copy, as (row, column)."""
    first = [(i, 8) for i in range(6)] + [(7, 8), (8, 8), (8, 7)] + [(8, 14 - i) for i in range(9, 15)]
    second = [(8, size - 1 - i) for i in range(8)] + [(size - 15 + i, 8) for i in range(8, 15)]
    return first + second


def penalty(m):
    """The standard's penalty score for a masked symbol (lower is better)."""
    size = len(m)
    score = 0
    for lines in (m, [list(col) for col in zip(*m)]):
        for line in lines:
            run = 1
            for i in range(1, size + 1):
                if i < size and line[i] == line[i - 1]:
                    run += 1
                else:
                    if run >= 5:
                        score += run - 2
                    run = 1
            s = "".join(map(str, line))
            score += 40 * (s.count("10111010000") + s.count("00001011101"))
    for r in range(size - 1):
        for c in range(size - 1):
            if m[r][c] == m[r][c + 1] == m[r + 1][c] == m[r + 1][c + 1]:
                score += 3
    dark = sum(map(sum, m))
    score += 10 * (abs(dark * 20 - size * size * 10) // (size * size))
    return score


if __name__ == "__main__":
    for row in encode(" ".join(sys.argv[1:]) or "HTTPS://EXAMPLE.COM"):
        print("".join("##" if b else "  " for b in row))
