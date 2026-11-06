#!/bin/bash
# The Amiga run rig works and cleans up after itself: boots a stock A500 under Amiberry with the FujiNet broker,
# runs test/amiga/dial.c against a scratch game server on 6599, sees the server's region frame, stops everything,
# and finds no process of ours alive and the three ports free afterwards.
set -u
cd "$(dirname "$0")/.."
RUN=${AMIGA_RUN_DIR:-build/amiga/run}
PORTS="6599 65501 23499"
fail() { echo "FAIL: $*"; exit 1; }

make build/amiga/dial >/dev/null || fail "make build/amiga/dial"
for p in $PORTS; do ss -ltn | grep -q ":$p\b" && fail "port $p is in use already; not touching it"; done

(cd ../server && exec python3 serve.py --host 127.0.0.1 --port 6599 >/dev/null 2>&1) &
SERVER=$!
trap 'tools/run-amiga.sh --stop >/dev/null 2>&1; kill $SERVER 2>/dev/null' EXIT

rm -f "$RUN/pids.stopped"
tools/run-amiga.sh build/amiga/dial || fail "run-amiga.sh did not start"
python3 tools/amiga_guest.py wait "$RUN/share/log" "tcp_open rc=0" 240 || fail "the guest never dialled"
python3 tools/amiga_guest.py wait "$RUN/share/log" "read rc=0 n=" 30 || fail "no frame came back"
grep -q "read rc=0 n=38 .* : 61 0C" "$RUN/share/log" || fail "the first frame was not the region 'a' frame: $(grep 'read rc' "$RUN/share/log" | head -1)"
python3 tools/amiga_guest.py shot "$(pwd)/$RUN/selfcheck.png" || fail "screenshot"

tools/run-amiga.sh --stop || fail "--stop"
kill $SERVER; wait $SERVER 2>/dev/null; trap - EXIT
sleep 1
[ -s "$RUN/pids.stopped" ] || fail "no record of the processes the rig started"
n=0
while read -r name pid _; do
    n=$((n + 1))
    kill -0 "$pid" 2>/dev/null && fail "$name (pid $pid) still alive"
done < "$RUN/pids.stopped"
[ "$n" -eq 3 ] || fail "expected 3 recorded processes (nio, amiberry, socat), found $n"
for p in $PORTS; do ss -ltn | grep -q ":$p\b" && fail "port $p still held"; done
echo "PASS: amiga rig"
