#!/usr/bin/env python3
"""Run a program on a pseudo terminal, for test/linux_accept.sh: drain what it draws, resize the terminal, signal
it, and report whether it left the terminal restored.

    linux_pty.py [--size ROWSxCOLS] [--resize SECS:ROWSxCOLS]... [--stop-when FILE:TEXT] [--signal NAME]
                     [--after SECS] [--exit-within SECS] [--expect TEXT]... [--timeout SECS] -- PROGRAM ARGS...

--stop-when: signal the program as soon as TEXT appears in FILE (the server's log); --after: signal it after SECS
instead. Exit 0 if the program ended by itself or on the signal with the terminal restored (the alternate screen
left, the cursor shown again), 1 otherwise; the last lines of what it drew are printed on failure."""
import argparse, fcntl, os, pty, select, signal, struct, subprocess, sys, termios, time


def winsz(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def seq(name):
    return subprocess.run(["tput", "-T", "xterm-256color", name], capture_output=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", default="30x100")
    ap.add_argument("--resize", action="append", default=[])
    ap.add_argument("--stop-when")
    ap.add_argument("--signal", default="SIGINT")
    ap.add_argument("--after", type=float)
    ap.add_argument("--exit-within", type=float)
    ap.add_argument("--expect", action="append", default=[])
    ap.add_argument("--timeout", type=float, default=300)
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
    rows, cols = map(int, a.size.split("x"))
    master, slave = pty.openpty()
    winsz(slave, rows, cols)
    env = dict(os.environ, TERM="xterm-256color", LANG="C.UTF-8", LC_ALL="C.UTF-8")
    p = subprocess.Popen(cmd, stdin=slave, stdout=slave, stderr=slave, env=env, start_new_session=True,
                         preexec_fn=lambda: fcntl.ioctl(0, termios.TIOCSCTTY, 0))
    os.close(slave)
    out = bytearray()
    resizes = sorted((float(r.split(":")[0]), r.split(":")[1]) for r in a.resize)
    t0 = time.time()
    signalled = None
    while True:
        r, _, _ = select.select([master], [], [], 0.1)
        if r:
            try:
                d = os.read(master, 65536)
            except OSError:
                d = b""
            if not d:
                break
            out += d
        now = time.time() - t0
        while resizes and now >= resizes[0][0]:
            rows, cols = map(int, resizes.pop(0)[1].split("x"))
            winsz(master, rows, cols)
            p.send_signal(signal.SIGWINCH)
        if signalled is None:
            hit = False
            if a.stop_when:
                f, text = a.stop_when.split(":", 1)
                try:
                    hit = text in open(f, errors="replace").read()
                except OSError:
                    pass
            if a.after is not None and now >= a.after:
                hit = True
            if hit:
                signalled = now
                p.send_signal(getattr(signal, a.signal))
        if (a.exit_within is not None and signalled is not None and p.poll() is None
                and now - signalled > a.exit_within):
            p.kill()
            print("FAIL: still running %.0fs after the signal (one signal must end it)" % a.exit_within)
            sys.exit(1)
        if p.poll() is not None and not r:
            break
        if now > a.timeout:
            p.kill()
            print("FAIL: timed out after %ds" % a.timeout)
            sys.exit(1)
    p.wait()
    ok = True
    why = []
    if signalled is None and a.stop_when:
        ok = False
        why.append("the program ended before %r appeared" % a.stop_when)
    if p.returncode not in (0, 130, -signal.SIGINT, -signal.SIGTERM) and p.returncode is not None:
        ok = False
        why.append("exit status %s" % p.returncode)
    for text in a.expect:
        if text.encode() not in out:
            ok = False
            why.append("%r was never drawn" % text)
    rmcup, cnorm = seq("rmcup"), seq("cnorm")
    if rmcup and rmcup not in out:
        ok = False
        why.append("the alternate screen was not left (rmcup missing)")
    if cnorm and cnorm.split(b"\x1b")[-1] not in out[-400:] and cnorm not in out[-400:]:
        ok = False
        why.append("the cursor was not shown again (cnorm missing)")
    print("exit status %s, %d bytes drawn, signalled at %s" % (p.returncode, len(out), "%.1fs" % signalled if signalled else "-"))
    if not ok:
        print("FAIL: " + "; ".join(why))
        print(out[-600:].decode("utf-8", "replace"))
        sys.exit(1)
    print("terminal restored")


main()
