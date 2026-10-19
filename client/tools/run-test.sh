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
#   tools/run-test.sh build/synack.atr               boot a disk image from FujiNet D1,
#       as the lobby would (works with HEADLESS and DUMPDIR too)
#   BASIC=1 tools/run-test.sh build/synack.atr        boot with the BASIC ROM on, as a real
#       XL/XE does unless OPTION is held (screen memory moves down to $9C40)
#   INSTANCE=1 tools/run-test.sh build/synack.xex     a second Atari at the same time
#       each instance gets its own NetSIO port (9997 + n) and FujiNet web page (8000 + n)
#
# Start the server first:  cd ../server && python3 serve.py --trace
# Closing the emulator (or Ctrl-C) stops FujiNet-PC as well.
set -e
XEX=$(realpath "$1")
ATARI800=${ATARI800:-atari800}
FUJINET_DIR=${FUJINET_DIR:-$HOME/Nextcloud/Projects/computers/atari8/Source/FujiNetWIFI/fujinet-firmware/build/dist}
INSTANCE=${INSTANCE:-0}
NETSIO=$((9997 + INSTANCE))
WEB=$((8000 + INSTANCE))
WORK=$(mktemp -d "${TMPDIR:-/tmp}/synack-run.XXXXXX")
mkdir "$WORK/sd"
awk -v netsio=$NETSIO '/^\[/ { skip = /^\[Mount[0-9]+\]/; boip = /^\[BOIP\]/ }
     /^configenabled=/ { $0 = "configenabled=0" }
     /^boot_mode=/ { $0 = "boot_mode=0" }
     boip && /^port=/ { $0 = "port=" netsio }
     !skip' "$FUJINET_DIR/fnconfig.ini" > "$WORK/fnconfig.ini"
case "$XEX" in
*.atr|*.ATR)
    # D1 = the image, from host slot 1 (the SD folder; hostslot counts from 1); the Atari boots from it
    cp "$XEX" "$WORK/sd/boot.atr"
    printf '\n[Host1]\ntype=SD\nname=SD\n\n[Mount1]\nhostslot=1\npath=/boot.atr\nmode=r\n' >> "$WORK/fnconfig.ini"
    RUN= ;;
*)  RUN="-run $XEX" ;;
esac

# Exit code 75 means "restart me" (the emulator resets FujiNet when it connects), as in run-fujinet.
(cd "$FUJINET_DIR" && while :; do
    rc=0
    stdbuf -oL ./fujinet -c "$WORK/fnconfig.ini" -s "$WORK/sd" -u "http://0.0.0.0:$WEB" || rc=$?
    [ $rc -eq 75 ] || break
done) > "$WORK/fujinet.log" 2>&1 &
FN=$!
trap 'pkill -P $FN 2>/dev/null; kill $FN 2>/dev/null; echo "logs: $WORK"' EXIT INT TERM

if [ -n "$BASIC" ]; then
    OPTS="-xl -basic -netsio $NETSIO"
    SCREEN="9C40 9FFF"          # GR.0 screen memory under the BASIC ROM
else
    OPTS="-xl -nobasic -netsio $NETSIO"
    SCREEN="BC40 BFFF"          # GR.0 screen memory with no BASIC
fi
if [ -n "$DUMPDIR" ]; then
    mkdir -p "$DUMPDIR"
    i=0
    while [ $i -lt 999 ]; do
        printf 'WRITE %s %s/scr%03d.bin\nCONT\n' "$SCREEN" "$DUMPDIR" $i
        i=$((i + 1))
    done > "$WORK/monitor.txt"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$ATARI800" $OPTS -nosound -bbrk $RUN \
        < "$WORK/monitor.txt" > "$WORK/atari800.log" 2>&1
elif [ -n "$HEADLESS" ]; then
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$ATARI800" $OPTS -nosound $RUN > "$WORK/atari800.log" 2>&1
else
    "$ATARI800" $OPTS $RUN
fi
