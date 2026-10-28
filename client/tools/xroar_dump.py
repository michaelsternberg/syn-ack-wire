#!/usr/bin/env python3
"""Save the CoCo autoplay build's screens from a running xroar, through its GDB port.

Usage:
    xroar_dump.py MAP OUTDIR [--port N] [--count N] [--wait SECONDS]
    xroar_dump.py --poll SECONDS OUTDIR [--port N] [--count N]

xroar runs with -gdb -gdb-port N (default 65520). MAP is the autoplay build's link map
(build/coco/auto/synack.map, from make coco-auto): this puts a breakpoint on its pc_dump
(test/coco_dump.c, which autoplay calls at each new prompt and every 4 seconds), and at each stop
saves the PMODE 4 screen ($0E00, 6144 bytes) to OUTDIR/scrNNN.bin, then lets the machine run on.
It stops after --count screens, or when none comes for --wait seconds, and detaches, leaving the
machine running. With --poll, no breakpoint: it stops the machine every SECONDS and saves the
screen when it has changed. tools/render_coco.py draws the screens."""
import argparse
import os
import re
import socket
import sys
import time

SCREEN, SIZE = 0x0E00, 6144


class Gdb:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port))
        self.buf = b""

    def send(self, data):
        pkt = b"$" + data + b"#" + b"%02x" % (sum(data) & 0xFF)
        self.s.sendall(pkt)

    def recv(self):
        """The next packet's body (acknowledging it), skipping acks."""
        while True:
            while b"#" not in self.buf or len(self.buf) < self.buf.index(b"#") + 3:
                chunk = self.s.recv(65536)
                if not chunk:
                    raise EOFError("xroar closed the GDB connection")
                self.buf += chunk
            self.buf = self.buf.lstrip(b"+")
            start = self.buf.find(b"$")
            if start < 0:
                self.buf = b""
                continue
            end = self.buf.find(b"#", start)
            if end < 0 or len(self.buf) < end + 3:
                continue
            body = self.buf[start + 1:end]
            self.buf = self.buf[end + 3:]
            self.s.sendall(b"+")
            return body

    def ask(self, data):
        self.send(data)
        return self.recv()

    def read(self, addr, n):
        out = b""
        while n:
            k = min(n, 1024)
            r = self.ask(b"m%x,%x" % (addr, k))
            if r.startswith(b"E"):
                raise IOError("reading %04X: %s" % (addr, r.decode()))
            out += bytes.fromhex(r.decode())
            addr += k
            n -= k
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("map", nargs="?")
    ap.add_argument("outdir")
    ap.add_argument("--poll", type=float)
    ap.add_argument("--port", type=int, default=65520)
    ap.add_argument("--count", type=int, default=1000)
    ap.add_argument("--wait", type=float, default=60)
    a = ap.parse_args()
    if a.poll:
        poll(a)
        return
    m = re.search(r"^Symbol: _pc_dump \S+ = ([0-9A-F]+)", open(a.map).read(), re.M)
    if not m:
        sys.exit("%s: no _pc_dump" % a.map)
    bp = int(m.group(1), 16)
    os.makedirs(a.outdir, exist_ok=True)
    g = Gdb(a.port)
    g.ask(b"?")                         # attaching stops the machine
    if g.ask(b"Z0,%x,1" % bp) != b"OK":
        sys.exit("xroar refused a breakpoint at %04X" % bp)
    g.s.settimeout(a.wait)
    n = 0
    try:
        while n < a.count:
            g.send(b"c")
            g.recv()                    # a stop: the breakpoint
            with open(os.path.join(a.outdir, "scr%03d.bin" % n), "wb") as f:
                f.write(g.read(SCREEN, SIZE))
            n += 1
    except socket.timeout:
        print("no screen for %g s" % a.wait)
    g.s.settimeout(5)
    try:
        if n < a.count:
            g.s.sendall(b"\x03")       # still running: stop it to take the breakpoint out
            g.recv()
        g.ask(b"z0,%x,1" % bp)
        g.send(b"D")
    except (socket.timeout, EOFError):
        pass
    print("%d screens in %s" % (n, a.outdir))


def poll(a):
    os.makedirs(a.outdir, exist_ok=True)
    g = Gdb(a.port)
    g.ask(b"?")
    last, n = None, 0
    while n < a.count:
        scr = g.read(SCREEN, SIZE)
        if scr != last:
            with open(os.path.join(a.outdir, "scr%03d.bin" % n), "wb") as f:
                f.write(scr)
            last, n = scr, n + 1
        g.send(b"c")
        time.sleep(a.poll)
        g.s.sendall(b"\x03")
        g.recv()
    g.send(b"D")
    print("%d screens in %s" % (n, a.outdir))


if __name__ == "__main__":
    main()
