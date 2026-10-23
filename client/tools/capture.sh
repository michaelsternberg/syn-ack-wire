#!/bin/sh
# Capture the manual's staged screens on atari800: for each scenario in docs/scenarios.toml, serve the staged
# game (server/stage.py), let the autoplay client open on it, keep the screen dump taken at the scenario's
# `capture` line, and redraw the captures (make screens in docs/manual/atari).
#
#   tools/capture.sh                     every scenario
#   tools/capture.sh tied-root spike     just these
#   PORT=6599 INSTANCE=7 tools/capture.sh    the defaults: the server's port, run-test.sh's instance
#   KEEP=1 tools/capture.sh tied-root    keep the dumps and logs (the script says where)
#
# The ports (PORT, NetSIO 9997+INSTANCE, web 8000+INSTANCE) must be free: the script stops otherwise.
# atari800 runs on a scratch copy of ~/.atari800.cfg, which it would otherwise rewrite on exit.
set -e
cd "$(dirname "$0")/.."
CLIENT=$(pwd)
ROOT=$(dirname "$CLIENT")
PORT=${PORT:-6599}
INSTANCE=${INSTANCE:-7}
SECONDS_EACH=${SECONDS_EACH:-35}
DUMPS=$ROOT/docs/manual/atari/screens/dumps

for p in $PORT $((9997 + INSTANCE)) $((8000 + INSTANCE)); do
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
make -s B="$WORK/b" URL="N:TCP://localhost:$PORT/" "$WORK/b/synack-auto.xex" > "$WORK/build.log" 2>&1 \
    || { cat "$WORK/build.log" >&2; exit 1; }

if [ $# -eq 0 ]; then
    set -- $(python3 -c 'import sys, tomllib; print(" ".join(tomllib.load(open(sys.argv[1], "rb"))))' \
             "$ROOT/docs/scenarios.toml")
fi

for sc in "$@"; do
    rm -rf "$WORK/d"; mkdir "$WORK/d"           # the dumps: $WORK/d/scrNNN.bin, kept short
    (cd "$ROOT/server" && exec python3 stage.py "$sc" --port "$PORT") > "$WORK/$sc.log" 2>&1 &
    server=$!
    sleep 1
    # BASIC=1: screen memory at $9C40, where it is with the BASIC ROM in, as on an 800 with the cartridge
    DUMPDIR="$WORK/d" BASIC=1 INSTANCE=$INSTANCE ATARI800="$WORK/a8" timeout "$SECONDS_EACH" \
        sh tools/run-test.sh "$WORK/b/synack-auto.xex" > "$WORK/$sc.run" 2>&1 || true
    kill $server 2>/dev/null || true
    wait $server 2>/dev/null || true
    python3 - "$ROOT/docs/scenarios.toml" "$sc" "$WORK/d" "$DUMPS" <<'EOF'
import glob, os, shutil, sys, tomllib
sys.path.insert(0, "tools")
from screen2txt import char
scenarios, name, dumps, out = sys.argv[1:]
all_ = tomllib.load(open(scenarios, "rb"))
sc = all_[name]
while "capture" not in sc and "like" in sc:
    sc = all_[sc["like"]]
want = sc["capture"].upper()
for f in sorted(glob.glob(os.path.join(dumps, "scr*.bin"))):
    d = open(f, "rb").read()
    rows = ["".join(char(b) for b in d[r * 40:r * 40 + 40]).upper().strip() for r in (22, 23)]
    if any(r.startswith(want) for r in rows):
        shutil.copy(f, os.path.join(out, name + ".bin"))
        print(f"{name}: {os.path.basename(f)}")
        break
else:
    sys.exit(f"{name}: no screen showed {want!r} (KEEP=1 keeps the dumps and logs)")
EOF
done

make -s -C "$ROOT/docs/manual/atari" screens
echo "captures redrawn in docs/manual/atari/screens"
