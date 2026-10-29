#!/bin/sh
# Run the NES client in MAME with its FujiNet NES cartridge (-nes_slot fujinet: a MAME built from a tree with
# the cartridge grafted in, ~/Workspace/mame by default) and FujiNet-PC (an RS232 build, which takes the
# cartridge's link on BoIP) for the network. MAME loads the image itself; FujiNet only provides the network.
# Your own fnconfig.ini is not touched: a scratch copy is used, with its own BoIP and web ports.
#
#   tools/run-nes.sh build/nes/syn-ack-wire-nes.nes           MAME's window
#   HEADLESS=1 AT=20 tools/run-nes.sh IMAGE                  no window: at 20 s print the screen as text,
#                                                           save a snapshot (SNAPDIR) and stop
#   PRESS="6:Up 9:Start 12:A" ...                           press the pad on a schedule (tools/nes.lua)
#   SHOTS="10 12.5" ...                                     snapshots at those seconds, into SNAPDIR
#   DUMPDIR=/tmp/shots SECS=300 tools/run-nes.sh build/nes/auto/syn-ack-wire-nes.nes
#       headless; the autoplay build's dumps go to DUMPDIR/scrNNN.bin (tools/render_nes.py shows them)
#   INSTANCE=1 tools/run-nes.sh IMAGE                       a second NES at the same time:
#       each instance gets its own BoIP port (9985 + n) and FujiNet web page (8020 + n)
#
# Start the server first:  cd ../server && python3 serve.py --trace
# Closing MAME (or Ctrl-C) stops FujiNet-PC as well.
set -e
NES=$(realpath "$1")
HERE=$(cd "$(dirname "$0")" && pwd)
MAME=${MAME:-$HOME/Workspace/mame}
FUJINET_DIR=${FUJINET_DIR:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
INSTANCE=${INSTANCE:-0}
BOIP=$((9985 + INSTANCE))
WEB=$((8020 + INSTANCE))
for p in $BOIP $WEB; do
    if ss -ltn | grep -q "[:.]$p "; then
        echo "port $p is in use: another INSTANCE?" >&2
        exit 1
    fi
done
WORK=$(mktemp -d "${TMPDIR:-/tmp}/synack-nes.XXXXXX")
mkdir "$WORK/sd"
tr -d '\r' < "$FUJINET_DIR/fnconfig.ini" | awk -v boip=$BOIP '
     /^\[/ { skip = /^\[Mount[0-9]+\]/; b = /^\[BOIP\]/ }
     /^configenabled=/ { $0 = "configenabled=0" }
     /^boot_mode=/ { $0 = "boot_mode=0" }
     b && /^host=/ { $0 = "host=127.0.0.1" }
     b && /^port=/ { $0 = "port=" boip }
     !skip' > "$WORK/fnconfig.ini"

(cd "$FUJINET_DIR" && stdbuf -oL ./fujinet -c "$WORK/fnconfig.ini" -s "$WORK/sd" -u "http://127.0.0.1:$WEB") \
    > "$WORK/fujinet.log" 2>&1 &
FN=$!
# set +e: the subshell may have become FujiNet-PC itself, and pkill finding no child must not end the trap
trap 'set +e; pkill -P $FN 2>/dev/null; kill $FN 2>/dev/null; echo "logs: $WORK"' EXIT INT TERM
sleep 2

LBL=${LBL:-$(dirname "$NES")/synack.lbl}
SNAPDIR=${SNAPDIR:-$WORK}
[ -n "$DUMPDIR" ] && mkdir -p "$DUMPDIR" && DUMPDIR=$(realpath "$DUMPDIR")
if [ -n "$HEADLESS$DUMPDIR$AT" ]; then
    VIDEO="-video none -sound none"
else
    VIDEO=
fi
# MAME must run from its own tree, or -autoboot_script is silently ignored.
cd "$MAME"
FUJINET_TCP=127.0.0.1:$BOIP NES_PRESS="$PRESS" NES_AT="$AT" NES_EVERY="$EVERY" NES_SHOTS="$SHOTS" \
    NES_DUMPDIR="$DUMPDIR" NES_LBL="$LBL" \
    ./mame nes -nes_slot fujinet -cart "$NES" -snapshot_directory "$SNAPDIR" -autoboot_script "$HERE/nes.lua" \
    $VIDEO ${SECS:+-seconds_to_run $SECS} 2>&1 | grep -v '^fujinet:'
