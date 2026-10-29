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
#   PLATFORM=nes tools/capture.sh        the NES: the autoplay image in MAME (tools/run-nes.sh), its dumps from nes.lua
#   PLATFORM=adam tools/capture.sh       the Adam: its display code on this PC (make pc-adam), no emulator
#   PLATFORM=h89 tools/capture.sh        the Heathkit H89: its display code on this PC (make pc-h89), no emulator
#   PLATFORM=intv tools/capture.sh       the Intellivision: the autoplay ROM in jzIntv (tools/run-intv.sh), its dumps
#                                        from intvsim.py at each new prompt
#   PORT=6599 INSTANCE=7 tools/capture.sh    the defaults: the server's port, run-test.sh's instance
#   KEEP=1 tools/capture.sh tied-root    keep the dumps and logs (the script says where)
#
# The ports (PORT; for the Atari also NetSIO 9997+INSTANCE, web 8000+INSTANCE; for the NES BoIP 9985+INSTANCE, web
# 8020+INSTANCE; for the Intellivision BoIP 9980+INSTANCE, web 8040+INSTANCE) must be free: the script stops
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
    apple2 | msdos | coco | adam | h89) ports=$PORT ;;
    nes) ports="$PORT $((9985 + INSTANCE)) $((8020 + INSTANCE))" ;;
    intv) ports="$PORT $((9980 + INSTANCE)) $((8040 + INSTANCE))" ;;
    *) echo "PLATFORM: atari, apple2, msdos, coco, nes, adam, h89 or intv" >&2; exit 1 ;;
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
if [ "$PLATFORM" = nes ]; then
    make -s NES="$WORK/nes" NESAUTO=1 URL="N:TCP://localhost:$PORT/" LOBBY_KEY=0 "$WORK/nes/syn-ack-wire-nes.nes" \
        > "$WORK/build.log" 2>&1 || { cat "$WORK/build.log" >&2; exit 1; }
elif [ "$PLATFORM" = intv ]; then
    make -s INTV="$WORK/intv" INTVAUTO=1 URL="N:TCP://127.0.0.1:$PORT/" LOBBY_KEY=0 "$WORK/intv/syn-ack-wire-intv.rom" \
        > "$WORK/build.log" 2>&1 || { cat "$WORK/build.log" >&2; exit 1; }
elif [ "$PLATFORM" != atari ]; then
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
sys.path.insert(0, "tools/h89")
from screen2txt import char
import render_adam, render_apple2, render_coco, render_intv, render_msdos, render_nes
import render as render_h89
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
    elif platform == "nes":     # the info and question rows are 26 and 27 of its name table
        rows = [line.upper().strip() for line in render_nes.text(d)[26:28]]
    elif platform == "adam":
        rows = [line.upper().strip() for line in render_adam.text(d)[22:24]]
    elif platform == "h89":
        rows = [line.upper().strip() for line in render_h89.text(d)[22:24]]
    elif platform == "intv":    # the info and question rows are 10 and 11 of its 12
        rows = [line.upper().strip() for line in render_intv.text(d)[10:12]]
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
# worked. The Apple II, MS-DOS, CoCo and H89 builds on this PC have one screen: one try.
order="1 0"
[ "$PLATFORM" != atari ] && order=0
run() {     # run <scenario> <basic: 1 or 0>: stage it, capture, pick; true if a dump matched
    rm -rf "$WORK/d"; mkdir "$WORK/d"           # the dumps: $WORK/d/scrNNN.bin, kept short
    (cd "$ROOT/server" && exec python3 stage.py "$1" --host 127.0.0.1 --port "$PORT") > "$WORK/$1.log" 2>&1 &
    server=$!
    sleep 1
    if [ "$PLATFORM" = nes ]; then
        DUMPDIR="$WORK/d" INSTANCE=$INSTANCE SECS=$SECONDS_EACH TMPDIR="$WORK" \
            sh tools/run-nes.sh "$WORK/nes/syn-ack-wire-nes.nes" > "$WORK/$1.run" 2>&1 || true
    elif [ "$PLATFORM" = intv ]; then
        echo "dumps d 999 $((SECONDS_EACH * 60))" | DUMPDIR="$WORK/d" INSTANCE=$INSTANCE SHOTDIR="$WORK/shots" \
            TMPDIR="$WORK" sh tools/run-intv.sh "$WORK/intv/syn-ack-wire-intv.rom" - > "$WORK/$1.run" 2>&1 || true
    elif [ "$PLATFORM" != atari ]; then
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
# Every platform but the Atari: the Atari manual's are captures from real play.
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
    if [ "$PLATFORM" = nes ]; then
        DUMPDIR="$WORK/d" INSTANCE=$INSTANCE SECS=20 TMPDIR="$WORK" \
            sh tools/run-nes.sh "$WORK/nes/syn-ack-wire-nes.nes" > "$WORK/live.run" 2>&1 || true
    elif [ "$PLATFORM" = intv ]; then
        echo "dumps d 999 1500" | DUMPDIR="$WORK/d" INSTANCE=$INSTANCE SHOTDIR="$WORK/shots" TMPDIR="$WORK" \
            sh tools/run-intv.sh "$WORK/intv/syn-ack-wire-intv.rom" - > "$WORK/live.run" 2>&1 || true
    else
        DUMPDIR="$WORK/d" timeout 15 build/pc/pc-$PLATFORM 127.0.0.1 "$PORT" > "$WORK/live.run" 2>&1 || true
    fi
    kill $pids 2>/dev/null || true
    wait $pids 2>/dev/null || true
    python3 - "$WORK/d" "$DUMPS" "$PLATFORM" "$@" <<'PICK'
import glob, os, shutil, sys
sys.path.insert(0, "tools")
sys.path.insert(0, "tools/h89")
import render_adam, render_apple2, render_coco, render_intv, render_msdos, render_nes
import render as render_h89
dumps, out, platform, *names = sys.argv[1:]
text = {"msdos": render_msdos.text, "coco": render_coco.text, "nes": render_nes.text,
        "adam": render_adam.text, "h89": render_h89.text, "intv": render_intv.text}.get(platform, render_apple2.text)
if platform == "nes":       # its own layout: the foot on row 27, the seats on 14-19, the log on 20-25
    ask, seats, log, hint = 27, slice(14, 20), slice(20, 26), "PICK A HANDLE"
elif platform == "adam":    # the NES's in 24 rows: the seats on 12-17, the log on 18-21
    ask, seats, log, hint = 23, slice(12, 18), slice(18, 22), "TYPE A HANDLE"
elif platform == "intv":    # 12 rows: the title's keys on 11; SEATS' seats on 1-6, the log on 7-9
    ask, seats, log, hint = 11, slice(1, 7), slice(7, 10), "ENTER: "
else:
    ask, seats, log, hint = 23, slice(13, 19), slice(19, 22), "TYPE A HANDLE"
wants = {   # name: what its screen shows
    "title": lambda t: t[ask].strip().startswith(hint) and any("LOBBY OPEN" in l for l in t),
    "lobby-join": lambda t: any(l.strip().startswith(">") for l in t[seats]) and any("S TO START" in l for l in t[log]),
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
