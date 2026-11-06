#!/usr/bin/env python3
"""Drive the emulated Amiga from the host (see run-amiga.sh), on the fujinet-nio workspace's amiga_emulator library.

  amiga_guest.py shot ABSOLUTE_PNG        a screenshot (Amiberry wants an absolute path)
  amiga_guest.py type TEXT                keys, with {return}, {ramiga+e} ... tokens (workspace keyboard.py)
  amiga_guest.py wait FILE TEXT SECONDS   0 once FILE (a file the guest writes) contains TEXT, 1 on timeout
  amiga_guest.py click X Y [right]        move the mouse to the guest screen pixel (X, Y) and press and release a button
"""
import os, sys, time

WS = os.environ.get("FUJINET_NIO_WS", os.path.expanduser("~/dev/fujinet-nio-workspace"))
sys.path.insert(0, os.path.join(WS, "tools"))
SOCK = os.environ.get("AMIBERRY_SOCK", "/run/user/%d/amiberry.sock" % os.getuid())


def _kb():
    from amiga_emulator.keyboard import Keyboard
    return Keyboard(SOCK)


def shot(path):
    if not os.path.isabs(path):
        raise SystemExit("shot: the path must be absolute: " + path)
    _kb().screenshot(path, settle=0.5)


def type_(text):
    _kb().type_text(text, delay=0.08)


def click(x, y, right=False):
    """Amiberry's SEND_MOUSE_ABS <x> <y> <buttons>: the position, then the button down, then up."""
    from amiga_emulator import ipc
    sock = ipc.find_socket()
    button = "2" if right else "1"
    for state in ("0", button, "0"):
        ipc.request(sock, "SEND_MOUSE_ABS", str(x), str(y), state)
        time.sleep(0.15)


def wait_for(path, text, timeout):
    end = time.time() + timeout
    while time.time() < end:
        try:
            if text in open(path, errors="replace").read():
                return True
        except OSError:
            pass
        time.sleep(1)
    return False


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["shot"] and len(a) == 2:
        shot(a[1])
    elif a[:1] == ["type"] and len(a) == 2:
        type_(a[1])
    elif a[:1] == ["click"] and len(a) in (3, 4):
        click(int(a[1]), int(a[2]), len(a) == 4)
    elif a[:1] == ["wait"] and len(a) == 4:
        sys.exit(0 if wait_for(a[1], a[2], float(a[3])) else 1)
    else:
        sys.exit(__doc__)
