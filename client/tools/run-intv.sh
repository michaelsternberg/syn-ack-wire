#!/bin/sh
# Run the Intellivision client in jzIntv (the FujiNet-patched build, ~/Workspace/jzintv-20200712-src) with
# FujiNet-PC (an RS-232 build, ~/Workspace/fujinet-pc-rs232/build/dist by default), which gives it the network
# over BoIP: FujiNet-PC listens, jzIntv's cartridge dials it. jzIntv loads the ROM itself, so FujiNet only
# provides the network. Your own fnconfig.ini is not touched: a scratch copy is used, with CONFIG off and its
# own BoIP and web ports and its own SD folder.
#
#   tools/run-intv.sh build/intv/syn-ack-wire-intv.rom SCENARIO   headless: tools/intvsim.py plays SCENARIO
#                                                                 (its presses, pictures and the screen as
#                                                                 text; "-" reads it from stdin), pictures
#                                                                 into SHOTDIR
#   WINDOW=1 tools/run-intv.sh ROM                                jzIntv's window, to play (sound through
#                                                                 PulseAudio; F7 types on the ECS keyboard,
#                                                                 F5 goes back to the controller)
#   ECS=1 ...                                                     an ECS attached (its keyboard)
#   APPKEYS=dir ...                                               the lobby's AppKeys: dir's *.key into SD/FujiNet/
#   INSTANCE=1 ...                                                a second Intellivision: BoIP 9980 + n, web 8040 + n
#
# Start the server first:  cd ../server && python3 serve.py --host 127.0.0.1 --port 6598 --trace
# and build the client to dial it:  make intv INTV=build/intv/t URL=N:TCP://127.0.0.1:6598/ LOBBY_KEY=0
set -e
ROM=$(realpath "$1")
SCENARIO=${2:--}
HERE=$(cd "$(dirname "$0")" && pwd)
FUJINET_DIR=${FUJINET_DIR:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
JZINTV_DIR=${JZINTV_DIR:-$HOME/Workspace/jzintv-20200712-src}
INSTANCE=${INSTANCE:-0}
BOIP=$((9980 + INSTANCE))
WEB=$((8040 + INSTANCE))
for p in $BOIP $WEB; do
    if ss -ltn | grep -q "[:.]$p "; then
        echo "port $p is in use: another INSTANCE?" >&2
        exit 1
    fi
done
WORK=$(mktemp -d "${TMPDIR:-/tmp}/synack-intv.XXXXXX")
SHOTDIR=${SHOTDIR:-$WORK/shots}
mkdir -p "$WORK/sd/FujiNet" "$SHOTDIR"
SHOTDIR=$(realpath "$SHOTDIR")
[ -n "$APPKEYS" ] && cp "$APPKEYS"/*.key "$WORK/sd/FujiNet/"
tr -d '\r' < "$FUJINET_DIR/fnconfig.ini" | awk -v boip=$BOIP '
     /^\[/ { skip = /^\[(Mount[0-9]+|BOIP)\]/ }
     /^configenabled=/ { $0 = "configenabled=0" }
     !skip
     END { print "[BOIP]"; print "enabled=1"; print "host=127.0.0.1"; print "port=" boip }' > "$WORK/fnconfig.ini"

(cd "$FUJINET_DIR" && exec stdbuf -oL ./fujinet -c "$WORK/fnconfig.ini" -s "$WORK/sd" -u "http://127.0.0.1:$WEB") \
    > "$WORK/fujinet.log" 2>&1 &
FN=$!
trap 'set +e; kill $FN 2>/dev/null; echo "logs: $WORK"' EXIT INT TERM
for _ in 1 2 3 4 5 6 7 8 9 10; do
    ss -ltn | grep -q "[:.]$BOIP " && break
    sleep 0.5
done
if [ -n "$WINDOW" ]; then
    ecs=-s0
    [ "$ECS" = 1 ] && ecs="-s1 -E $JZINTV_DIR/rom/ecs.bin"
    SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-pulseaudio} "$JZINTV_DIR/bin/linux/jzintv" -z 3 \
        -e "$JZINTV_DIR/rom/exec.bin" -g "$JZINTV_DIR/rom/grom.bin" $ecs --fujinet=127.0.0.1:$BOIP "$ROM" \
        > "$WORK/jzintv.log" 2>&1 || true
else
    JZINTV_DIR=$JZINTV_DIR FUJINET=127.0.0.1:$BOIP python3 "$HERE/intvsim.py" "$ROM" "$SCENARIO" "$SHOTDIR"
    echo "pictures: $SHOTDIR"
fi
