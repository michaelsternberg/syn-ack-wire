#!/bin/bash
# Acceptance for the linux client: the autoplay build plays a whole game, through a POSIX fujinet-nio on a PTY, on
# a scratch server; then a mid-game resize and a signal must leave the terminal as it was.
#   test/linux_accept.sh
#   FUJINET_NIO=/path/to/fujinet-nio   the POSIX (PTY) build; default: the fujinet-nio workspace's fujibus-pty-debug
#   WAIT=300                           seconds to wait for the game to end
# Ports: the game server on 6599 (must be free). Everything else is private to a scratch directory.
set -u
cd "$(dirname "$0")/.."
PORT=6599
WAIT=${WAIT:-300}
NIO=${FUJINET_NIO:-$HOME/dev/fujinet-nio-workspace/repos/fujinet-nio/build/fujibus-pty-debug/fujinet-nio}
BIN=build/linux/synack-linux-auto
SERVER_MARK="linux_server.py --host 127.0.0.1 --port $PORT"
ss -ltn | grep -q ":$PORT\b" && { echo "port $PORT is in use: stopping, not touching it"; exit 2; }
[ -x "$NIO" ] || { echo "FAIL: no fujinet-nio at $NIO (build its fujibus-pty-debug target, or set FUJINET_NIO)"; exit 1; }
make -s linux-test >/dev/null || { echo "FAIL: the autoplay build"; exit 1; }

WORK=$(mktemp -d /tmp/nla.XXXXXX)
SERVER_PID= NIO_PID= SERVER2_PID=
cleanup() {
    for p in "$NIO_PID" "$SERVER_PID" "$SERVER2_PID"; do
        [ -n "$p" ] && [ -r "/proc/$p/cmdline" ] && kill "$p" 2>/dev/null
    done
    wait 2>/dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT
fail() { echo "FAIL: $*"; echo "--- server log"; tail -5 "$WORK/server.log"; echo "--- fujinet-nio log"; tail -8 "$WORK/nio.log"; exit 1; }

cp test/amiga_server.py "$WORK/linux_server.py"      # serve.py, saying GAME OVER in its log
sed -i 's|os.path.dirname(os.path.abspath(__file__)), "..", "..", "server"|"'"$PWD"'/../server"|' "$WORK/linux_server.py"
python3 "$WORK/linux_server.py" --host 127.0.0.1 --port $PORT --bot-delay 0.05 --trace >"$WORK/server.log" 2>&1 &
SERVER_PID=$!

# fujinet-nio on a PTY of its own: its working directory is the scratch one (it keeps ./fujinet-data there)
mkdir -p "$WORK/nio/fujinet-data"
cat >"$WORK/nio/fujinet-data/fujinet.yaml" <<YAML
fujinet:
  device_name: fuji-nio
channel:
  pty_path: $WORK/pty
YAML
(cd "$WORK/nio" && exec "$NIO") >"$WORK/nio.log" 2>&1 &
NIO_PID=$!
for i in $(seq 50); do [ -e "$WORK/pty" ] && break; sleep 0.2; done
[ -e "$WORK/pty" ] || fail "fujinet-nio made no PTY at $WORK/pty"
sleep 1

echo "1. a whole game, then SIGINT"
python3 test/linux_pty.py --timeout "$WAIT" --stop-when "$WORK/server.log:GAME OVER" --signal SIGINT -- \
    $BIN --port "$WORK/pty" --server 127.0.0.1:$PORT || fail "the game run"
grep -q "GAME OVER" "$WORK/server.log" || fail "the game did not end"

echo "2. a resize, down below the minimum and back, mid-game, then SIGTERM"
python3 test/linux_pty.py --timeout 60 --resize 8:20x30 --resize 12:30x100 --after 18 --signal SIGTERM --expect "TERMINAL TOO SMALL" -- \
    $BIN --port "$WORK/pty" --server 127.0.0.1:$PORT || fail "the resize run"

echo "3. the server dies mid-game: it must say so, and end on one signal while redialling"
DEAD=6598
ss -ltn | grep -q ":$DEAD\b" && fail "port $DEAD is in use: stopping, not touching it"
python3 "$WORK/linux_server.py" --host 127.0.0.1 --port $DEAD --bot-delay 0.05 >"$WORK/server2.log" 2>&1 &
SERVER2_PID=$!
sleep 1
( sleep 10; [ -r "/proc/$SERVER2_PID/cmdline" ] && kill "$SERVER2_PID" ) &
python3 test/linux_pty.py --timeout 60 --after 18 --signal SIGTERM --exit-within 3 --expect "REDIALING" -- \
    $BIN --port "$WORK/pty" --server 127.0.0.1:$DEAD || fail "the dead-server run"

echo "PASS"
