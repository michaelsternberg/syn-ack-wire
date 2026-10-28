#!/bin/sh
# Capture the manual's staged screens: for each scenario in docs/scenarios.toml, serve the staged game
# (server/stage.py), let the autoplay client open on it, keep the screen dump taken at the scenario's `capture`
# line, and redraw the captures (make screens in docs/manual/<platform>).
#
#   tools/capture.sh                     every scenario, on the Atari (atari800 + FujiNet-PC)
#   tools/capture.sh tied-root spike     just these
#   PLATFORM=apple2 tools/capture.sh     the Apple II: its display code on this PC (make pc-apple2), no emulator
#   PLATFORM=msdos tools/capture.sh      MS-DOS: its display code on this PC (make pc-msdos), no emulator
#   PLATFORM=coco tools/capture.sh       the CoCo 1/2: its display code on this PC (make pc-coco), no emulator
#   PORT=6599 INSTANCE=7 tools/capture.sh    the defaults: the server's port, run-test.sh's instance
#   KEEP=1 tools/capture.sh tied-root    keep the dumps and logs (the script says where)
#
# The ports (PORT; for the Atari also NetSIO 9997+INSTANCE, web 8000+INSTANCE) must be free: the script stops
# otherwise. atari800 runs on a scratch copy of ~/.atari800.cfg, which it would otherwise rewrite on exit.
set -e
cd "$(dirname "$0")/.."
CLIENT=$(pwd)
ROOT=$(dirname "$CLIENT")
PORT=${PORT:-6599}
INSTANCE=${INSTANCE:-7}
SECONDS_EACH=${SECONDS_EACH:-35}
PLATFORM=${PLATFORM:-atari}
DUMPS=$ROOT/docs/manual/$PLATFORM/screens/dumps
case $PLATFORM in
    atari) ports="$PORT $((9997 + INSTANCE)) $((8000 + INSTANCE))" ;;
    apple2 | msdos | coco) ports=$PORT ;;
    *) echo "PLATFORM: atari, apple2, msdos or coco" >&2; exit 1 ;;
esac
mkdir -p "$DUMPS"

for p in $ports; do
    if ss -ltn | grep -q ":$p\b"; then
        echo "port $p is in use: pick another PORT or INSTANCE" >&2
        exit 1
    fi
done

WORK=$(mktemp -d /tmp/sc.XXXXXX)        # short: atari800's monitor drops a dump whose path is too long
[ -n "$KEEP" ] && echo "work: $WORK" || trap 'rm -rf "$WORK"' EXIT   # KEEP=1: keep dumps and logs
[ -f "$HOME/.atari800.cfg" ] && cp "$HOME/.atari800.cfg" "$WORK/atari800.cfg"
printf '#!/bin/sh\nexec %s -config %s "$@"\n' "${ATARI800:-atari800}" "$WORK/atari800.cfg" > "$WORK/a8"
chmod +x "$WORK/a8"

echo "building the autoplay client for port $PORT"
if [ "$PLATFORM" != atari ]; then
    make -s pc-$PLATFORM > "$WORK/build.log" 2>&1 || { cat "$WORK/build.log" >&2; exit 1; }
else
    make -s B="$WORK/b" URL="N:TCP://localhost:$PORT/" "$WORK/b/synack-auto.xex" > "$WORK/build.log" 2>&1 \
        || { cat "$WORK/build.log" >&2; exit 1; }
fi

all=
if [ $# -eq 0 ]; then
    all=1
    set -- $(python3 -c 'import sys, tomllib; print(" ".join(tomllib.load(open(sys.argv[1], "rb"))))' \
             "$ROOT/docs/scenarios.toml")
fi

# The dump picker: copies the first dump whose prompt (or info) line starts with the scenario's `capture`, or its
# `capture_<platform>` where that platform's prompt words differ.
cat > "$WORK/pick.py" <<'EOF'
import glob, os, shutil, sys, tomllib
sys.path.insert(0, "tools")
from screen2txt import char
import render_apple2, render_coco, render_msdos
scenarios, name, dumps, out, platform = sys.argv[1:]
all_ = tomllib.load(open(scenarios, "rb"))
sc = all_[name]
own = "capture_" + platform
while own not in sc and "capture" not in sc and "like" in sc:
    sc = all_[sc["like"]]
want = sc.get(own, sc.get("capture")).upper()
for f in sorted(glob.glob(os.path.join(dumps, "scr*.bin"))):
    d = open(f, "rb").read()
    if platform == "apple2":
        rows = [line.upper().strip() for line in render_apple2.text(d)[22:24]]
    elif platform == "msdos":
        rows = [line.upper().strip() for line in render_msdos.text(d)[22:24]]
    elif platform == "coco":
        rows = [line.upper().strip() for line in render_coco.text(d)[22:24]]
    else:
        rows = ["".join(char(b) for b in d[r * 40:r * 40 + 40]).upper().strip() for r in (22, 23)]
    if any(r.startswith(want) for r in rows):
        shutil.copy(f, os.path.join(out, name + ".bin"))
        print(f"{name}: {os.path.basename(f)}")
        break
else:
    sys.exit(f"{name}: no screen showed {want!r} (KEEP=1 keeps the dumps and logs)")
EOF

# Where the Atari's screen memory is depends on the emulated machine: $9C40 with the BASIC ROM in (BASIC=1), else
# $BC40. That follows the owner's atari800 settings, so try one, fall back to the other, and keep the one that
# worked. The Apple II, MS-DOS and CoCo builds on this PC have one screen: one try.
order="1 0"
[ "$PLATFORM" != atari ] && order=0
run() {     # run <scenario> <basic: 1 or 0>: stage it, capture, pick; true if a dump matched
    rm -rf "$WORK/d"; mkdir "$WORK/d"           # the dumps: $WORK/d/scrNNN.bin, kept short
    (cd "$ROOT/server" && exec python3 stage.py "$1" --host 127.0.0.1 --port "$PORT") > "$WORK/$1.log" 2>&1 &
    server=$!
    sleep 1
    if [ "$PLATFORM" != atari ]; then
        DUMPDIR="$WORK/d" timeout "$SECONDS_EACH" build/pc/pc-$PLATFORM 127.0.0.1 "$PORT" > "$WORK/$1.run" 2>&1 || true
    else
        basic=; [ "$2" = 1 ] && basic=1
        DUMPDIR="$WORK/d" BASIC=$basic INSTANCE=$INSTANCE ATARI800="$WORK/a8" timeout "$SECONDS_EACH" \
            sh tools/run-test.sh "$WORK/b/synack-auto.xex" > "$WORK/$1.run" 2>&1 || true
    fi
    kill $server 2>/dev/null || true
    wait $server 2>/dev/null || true
    python3 "$WORK/pick.py" "$ROOT/docs/scenarios.toml" "$1" "$WORK/d" "$DUMPS" "$PLATFORM"
}

# The title screen and joining a lobby aren't staged games: a plain server with two test clients waiting in its
# lobby, ZER0COOL hosting (it waits for more players than will come) and MAMBA, then the client joins.
# The Apple II, MS-DOS and CoCo only: the Atari manual's are captures from real play.
live() {
    rm -rf "$WORK/d"; mkdir "$WORK/d"
    (cd "$ROOT/server" && exec python3 serve.py --host 127.0.0.1 --port "$PORT") > "$WORK/live.log" 2>&1 &
    pids=$!
    sleep 1
    for who in ZER0COOL MAMBA; do
        (cd "$ROOT/server" && exec python3 testclient.py --port "$PORT" --name $who --auto --humans 9 --quiet) \
            >> "$WORK/live.log" 2>&1 &
        pids="$pids $!"
        sleep 1
    done
    DUMPDIR="$WORK/d" timeout 15 build/pc/pc-$PLATFORM 127.0.0.1 "$PORT" > "$WORK/live.run" 2>&1 || true
    kill $pids 2>/dev/null || true
    wait $pids 2>/dev/null || true
    python3 - "$WORK/d" "$DUMPS" "$PLATFORM" "$@" <<'PICK'
import glob, os, shutil, sys
sys.path.insert(0, "tools")
import render_apple2, render_coco, render_msdos
dumps, out, platform, *names = sys.argv[1:]
text = {"msdos": render_msdos.text, "coco": render_coco.text}.get(platform, render_apple2.text)
wants = {   # name: what its screen shows
    "title": lambda t: t[23].strip().startswith("TYPE A HANDLE") and any("LOBBY OPEN" in l for l in t),
    "lobby-join": lambda t: any(l.startswith(">") for l in t[13:19]) and any("S TO START" in l for l in t[19:22]),
}
for name in names:
    for f in sorted(glob.glob(os.path.join(dumps, "scr*.bin"))):
        if wants[name]([l.upper() for l in text(open(f, "rb").read())]):
            shutil.copy(f, os.path.join(out, name + ".bin"))
            print(f"{name}: {os.path.basename(f)}")
            break
    else:
        sys.exit(f"{name}: no screen showed it (KEEP=1 keeps the dumps and logs)")
PICK
}

if [ "$PLATFORM" != atari ]; then
    live_names=
    rest=
    for sc in "$@"; do
        case $sc in title | lobby-join) live_names="$live_names $sc" ;; *) rest="$rest $sc" ;; esac
    done
    [ -n "$all" ] && live_names="title lobby-join"
    [ -n "$live_names" ] && live $live_names
    set -- $rest
fi

for sc in "$@"; do
    found=
    for b in $order; do
        if run "$sc" "$b" 2>/dev/null; then
            found=1
            order=$b
            break
        fi
    done
    if [ -z "$found" ]; then
        echo "$sc: no screen showed its capture line (KEEP=1 keeps the dumps and logs)" >&2
        exit 1
    fi
done

make -s -C "$ROOT/docs/manual/$PLATFORM" screens
echo "captures redrawn in docs/manual/$PLATFORM/screens"
