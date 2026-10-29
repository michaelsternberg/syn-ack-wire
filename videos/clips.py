#!/usr/bin/env python3
"""clips.py -- film the teaser's gameplay: the Intellivision client in jzIntv (client/tools/run-intv.sh, headless)
against a server of our own on this machine, each frame's screen recorded as a dump (intvsim's rec).

usage: clips.py [NAME...]      all the clips, or those; build/clips/NAME/f*.bin and build/clips/NAME.log

Each clip is a server (a staged game from docs/scenarios.toml, or a plain one with testclient.py players) on
127.0.0.1:PORT and a scenario of presses (see intvsim.py). The ROM is the film build (the Makefile's rom target):
the client dialing that port. The handle is typed on the ECS. Ports in use stop it: PORT, and the FujiNet-PC
copy's BoIP and web ports for INSTANCE (9980 + n, 8040 + n).
"""
import os
import re
import socket
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
CLIENT = os.path.join(REPO, "client")
SERVER = os.path.join(REPO, "server")
BUILD = os.path.join(HERE, "build")
ROM = os.path.join(BUILD, "rom", "syn-ack-wire-intv.rom")
PORT = int(os.environ.get("PORT", "6598"))
INSTANCE = int(os.environ.get("INSTANCE", "2"))


def typed(text, gap=9):
    """Scenario lines typing text on the ECS a key at a time, a human's pace (\\n: RTN)."""
    out, i = [], 0
    while i < len(text):
        k = text[i:i + 2] if text[i] == "\\" else text[i]
        i += len(k)
        out += ["ecs " + k, "wait %d" % gap]
    return out


JACK_IN = ["until NEO-KANSAI 1200"] + typed("ZER0COOL\\n", 2) + ["until SEATS? 1200", "key 3"]

CLIPS = {
    # The title, the handle typed on the ECS, hosting: two netrunners arrive while SEATS? waits (each arrival asks
    # again, which brings GRID back: SEATS is shown once both are in); three seats.
    "jackin": dict(server=["serve.py"], join=["MAMBA", "RAZORGRL"], scen=[
        "until WAITING 1200", "rec jackin 2", "wait 420"] + typed("ZER0COOL\\n") + [
        "until SEATS? 600", "wait 400", "btn R", "wait 300", "key 3", "wait 420", "rec off"]),
    # Spike: the hints; 2 runs C6, an infiltration; broker it to line 5; buy; the three views. Timed to the
    # teaser's four 4-bar scenes (448 frames each, from rec): spike at 380, broker at 678, buy at 966-1150, the
    # views at 1384, 1534, 1684.
    "turn": dict(server=["stage.py", "spike"], scen=JACK_IN + [
        "until EXPLOIT? 1800", "rec turn 2", "wait 380", "key 2", "wait 291", "key 5", "wait 281", "key 5",
        "wait 43", "key 5", "wait 43", "key 6", "wait 77", "key E", "wait 227", "btn R", "wait 143", "btn R",
        "wait 143", "btn R", "wait 150", "rec off"]),
    # The hivejack: 4 runs G3 between Lotus-12 (4) and Iron Vector (3); the log on SEATS; cash out 2, swap 2.
    # Timed to its 7-bar scene (784 frames): the spike at 60, cash out at 424, swap at 568.
    "hivejack": dict(server=["stage.py", "spike"], scen=JACK_IN + [
        "until EXPLOIT? 1800", "rec hivejack 2", "wait 60", "key 4", "wait 150", "btn R", "wait 200", "key 2",
        "wait 30", "key E", "wait 100", "key 2", "wait 30", "key E", "wait 180", "text", "rec off"]),
    # Buying with the ECS: syndicate letters and RTN; then the arrow keys step the views.
    "ecsbuy": dict(server=["stage.py", "spread-buy"], scen=JACK_IN + [
        "until 1-7 1800", "rec ecsbuy 2", "wait 120"] + typed("LLI", 40) + ["wait 30"] + typed("\\n", 120)
        + typed("\\r\\r", 150) + ["rec off"]),
    # The end: Lockdown, the null-route called, the final cash out, TOP:, the seats' new yen.
    "endgame": dict(server=["stage.py", "nullroute"], scen=JACK_IN + [
        "until NULL-ROUTE? 1800", "rec endgame 2", "wait 150", "key 1", "until TOP: 900", "wait 180", "btn R",
        "wait 360", "text", "rec off"]),
}


def port_free(p):
    with socket.socket() as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)    # a listener, not TIME-WAIT, makes it busy
        try:
            s.bind(("127.0.0.1", p))
            return True
        except OSError:
            return False


def wait_for(path, pattern, timeout=30):
    t = time.time()
    while time.time() - t < timeout:
        if os.path.exists(path) and re.search(pattern, open(path).read()):
            return True
        time.sleep(0.2)
    return False


def film(name, c):
    for p in (PORT, 9980 + INSTANCE, 8040 + INSTANCE):
        t = time.time()
        while not port_free(p):                         # the last clip's FujiNet-PC may still be going
            if time.time() - t > 20:
                raise SystemExit("port %d is in use: not filming (PORT, INSTANCE)" % p)
            time.sleep(0.5)
    out = os.path.join(BUILD, "clips")
    rec = os.path.join(out, name)
    os.makedirs(rec, exist_ok=True)
    for f in os.listdir(rec):
        os.remove(os.path.join(rec, f))
    tmp = os.path.join(BUILD, "tmp")
    os.makedirs(tmp, exist_ok=True)
    slog = os.path.join(out, name + ".server.log")
    procs = []
    with open(slog, "w") as log:
        procs.append(subprocess.Popen(["python3"] + c["server"] + ["--host", "127.0.0.1", "--port", str(PORT)],
                                      cwd=SERVER, stdout=log, stderr=subprocess.STDOUT))
    try:
        if not wait_for(slog, "listening"):
            raise SystemExit("%s: the server didn't start (%s)" % (name, slog))

        def joiners():                                  # the others arrive once the Intellivision has jacked in
            if not wait_for(slog, "as ZER0COOL", 600):
                return
            for k, who in enumerate(c.get("join", [])):
                time.sleep(2.5 if k == 0 else 2.0)
                procs.append(subprocess.Popen(
                    ["python3", "testclient.py", "--host", "127.0.0.1", "--port", str(PORT), "--name", who,
                     "--auto", "--quiet"], cwd=SERVER, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        th = threading.Thread(target=joiners, daemon=True)
        th.start()
        scen = os.path.join(out, name + ".scen")
        with open(scen, "w") as f:
            f.write("\n".join(c["scen"]) + "\n")
        env = dict(os.environ, ECS="1", INSTANCE=str(INSTANCE), TMPDIR=tmp, SHOTDIR=out)
        t = time.time()
        with open(os.path.join(out, name + ".log"), "w") as log:
            r = subprocess.run(["tools/run-intv.sh", ROM, scen], cwd=CLIENT, env=env, stdout=log,
                               stderr=subprocess.STDOUT, timeout=1800)
        n = len([f for f in os.listdir(rec) if f.endswith(".bin")])
        print("%-9s %4d frames recorded in %.0f s (exit %d)" % (name, n, time.time() - t, r.returncode))
    finally:
        for p in procs:
            p.terminate()
        for p in procs:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()


def main():
    if not os.path.exists(ROM):
        raise SystemExit("no %s: make rom" % ROM)
    want = sys.argv[1:] or list(CLIPS)
    for name in want:
        film(name, CLIPS[name])


if __name__ == "__main__":
    main()
