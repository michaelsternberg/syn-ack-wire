#!/bin/sh
# Run an .xex on atari800 with FujiNet-PC on NetSIO for the network. atari800
# loads the program itself (-run); FujiNet's config disk is turned off. Your own
# fnconfig.ini is not touched: a scratch copy is used.
#
#   tools/run-test.sh build/synack.xex               emulator window
#   HEADLESS=1 tools/run-test.sh build/wiremon-auto.xex   no window, no sound
#   ATARI800=/path/to/atari800 tools/run-test.sh ...      another emulator build
#   DUMPDIR=/tmp/shots tools/run-test.sh build/synack-auto.xex
#       headless; every BRK the program executes saves the text screen to
#       DUMPDIR/scrNNN.bin (tools/screen2txt.py shows them)
#
# Start the server first:  cd ../server && python3 serve.py --trace
# Closing the emulator (or Ctrl-C) stops FujiNet-PC as well.
set -e
XEX=$(realpath "$1")
ATARI800=${ATARI800:-atari800}
FUJINET_DIR=${FUJINET_DIR:-$HOME/Nextcloud/Projects/computers/atari8/Source/FujiNetWIFI/fujinet-firmware/build/dist}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/synack-run.XXXXXX")
mkdir "$WORK/sd"
awk '/^\[/ { skip = /^\[Mount[0-9]+\]/ }
     /^configenabled=/ { $0 = "configenabled=0" }
     /^boot_mode=/ { $0 = "boot_mode=0" }
     !skip' "$FUJINET_DIR/fnconfig.ini" > "$WORK/fnconfig.ini"

# Exit code 75 means "restart me" (the emulator resets FujiNet when it connects), as in run-fujinet.
(cd "$FUJINET_DIR" && while :; do
    rc=0
    stdbuf -oL ./fujinet -c "$WORK/fnconfig.ini" -s "$WORK/sd" || rc=$?
    [ $rc -eq 75 ] || break
done) > "$WORK/fujinet.log" 2>&1 &
FN=$!
trap 'pkill -P $FN 2>/dev/null; kill $FN 2>/dev/null; echo "logs: $WORK"' EXIT INT TERM

OPTS="-xl -nobasic -netsio"
if [ -n "$DUMPDIR" ]; then
    # GR.0 screen memory with no BASIC: $BC40-$BFFF
    mkdir -p "$DUMPDIR"
    i=0
    while [ $i -lt 999 ]; do
        printf 'WRITE BC40 BFFF %s/scr%03d.bin\nCONT\n' "$DUMPDIR" $i
        i=$((i + 1))
    done > "$WORK/monitor.txt"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$ATARI800" $OPTS -nosound -bbrk -run "$XEX" \
        < "$WORK/monitor.txt" > "$WORK/atari800.log" 2>&1
elif [ -n "$HEADLESS" ]; then
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$ATARI800" $OPTS -nosound -run "$XEX" > "$WORK/atari800.log" 2>&1
else
    "$ATARI800" $OPTS -run "$XEX"
fi
