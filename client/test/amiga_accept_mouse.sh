#!/bin/bash
# Acceptance by mouse on the emulated stock A500: the autoclick build plays a whole game against bots on a scratch
# server, answering every prompt it can by a press on the widget that gives the key (test/amiga_autoclick.c), and logs
# each click; the game must end, and the prompts that came up must have been answered by clicks.
#   test/amiga_accept_mouse.sh            PAL (the default Amiberry machine)
#   NTSC=1 test/amiga_accept_mouse.sh     NTSC
#   CUT=1 test/amiga_accept_mouse.sh      also cut the link mid-game: the client must redial and carry on
# Needs the same things as tools/run-amiga.sh. Ports: game 6599, NIO 65501, serial 23499 (all must be free).
set -u
cd "$(dirname "$0")/.."
RUN=build/amiga/run
PORT=6599
WAIT=${WAIT:-900}
SERVER_MARK="amiga_server.py --host 127.0.0.1 --port $PORT"
LOG=build/amiga/server.log
fail() { echo "FAIL: $*"; cleanup; exit 1; }
SERVER_PID=
cleanup() {
    tools/run-amiga.sh --stop >/dev/null 2>&1
    if [ -n "$SERVER_PID" ] && [ -r "/proc/$SERVER_PID/cmdline" ] && tr '\0' ' ' < "/proc/$SERVER_PID/cmdline" | grep -qF -- "$SERVER_MARK"; then
        kill "$SERVER_PID"
    fi
}
for p in $PORT 65501 23499; do
    ss -ltn | grep -q ":$p\b" && { echo "port $p is in use: stopping, not touching it"; exit 2; }
done
rm -f build/amiga/synack-amiga-mouse   # never reuse a binary built for another URL
make -s amiga-test-mouse URL="N:TCP://127.0.0.1:${URLPORT:-$PORT}/" >/dev/null || fail "the autoclick build"
mkdir -p build/amiga
python3 test/amiga_server.py --host 127.0.0.1 --port $PORT --bot-delay 0.05 --trace >$LOG 2>&1 &
SERVER_PID=$!
sleep 1
[ -n "${NTSC:-}" ] && export AMIGA_NTSC=true
# USE_PREFS=1 URLPORT=6598: the client is built for a port nothing listens on and finds the scratch server only through
# its prefs file (SERVER=), which proves the setting and cannot reach any other server
[ -n "${USE_PREFS:-}" ] && export AMIGA_PREFS_SERVER="TCP://127.0.0.1:$PORT/"
tools/run-amiga.sh build/amiga/synack-amiga-mouse >$RUN/rig.out 2>&1 || { cat $RUN/rig.out; fail "the rig did not start"; }
shot() { python3 tools/amiga_guest.py shot "$PWD/$RUN/$1.png" >/dev/null 2>&1; }
t=0; cut=0
while [ $t -lt $WAIT ]; do
    sleep 5; t=$((t + 5))
    [ $((t % 60)) -eq 0 ] && shot "play-$t"
    grep -qE "GAME OVER" $LOG 2>/dev/null && break
    if [ -n "${CUT:-}" ] && [ $cut -eq 0 ] && [ $t -ge 60 ]; then
        cut=1
        echo "cutting the link for ${CUT_SECS:-30} s"
        pid=$(awk '$1=="socat"{print $2}' $RUN/pids)
        kill -STOP "$pid"; sleep ${CUT_SECS:-30}; kill -CONT "$pid"
    fi
done
shot final
grep -q "GAME OVER" $LOG || fail "the game did not end in ${WAIT}s"
clicks=$(grep -c '^click' $RUN/share/log 2>/dev/null)
for k in P B; do
    grep -q "^click $k " $RUN/share/log || fail "no $k prompt was answered by a click"
done
echo "clicks by prompt: $(grep '^click' $RUN/share/log | awk '{print $2}' | sort | uniq -c | tr '\n' ' ')"
cleanup
echo "PASS: the game ended after about ${t}s, $clicks clicks"
