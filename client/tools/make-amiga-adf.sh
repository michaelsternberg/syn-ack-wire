#!/bin/bash
# The Amiga floppy: build/amiga/syn-ack-wire-amiga-nio.adf (880 KB, OFS, volume SynAckWire), for the FujiNet-NIO
# firmware. It holds the game and its icon, nio/ (the FujiNet NIO device and its loader, for the Workbench in
# FUJINET_NIO_ADF_WB: wb13, wb31 or wb32; default wb13, the stock A500's) and a Start script that loads the device
# and runs the game: boot your own Workbench, then from a Shell or CLI   Execute SynAckWire:Start
# It doesn't boot by itself: on Kickstart 1.3 the device needs serial.device, which is on the Workbench disk and
# not ours to ship. The game is built for the public server (the default URL) unless URL=N:TCP://host:port/ is given.
set -eu
cd "$(dirname "$0")/.."
WS=${FUJINET_NIO_WS:-$HOME/dev/fujinet-nio-workspace}
PKG=$WS/build/amiga-artifacts/${FUJINET_NIO_ADF_WB:-wb13}/NIO
for f in "$PKG/fujinet-nio.device" "$PKG/fujinet-load-resident"; do
    [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done
command -v xdftool >/dev/null || { echo "needs xdftool (amitools)" >&2; exit 1; }
make -s amiga ${URL:+URL="$URL"}
GAME=syn-ack-wire-amiga-nio
VOL=SynAckWire
D=build/amiga/adf
ADF=build/amiga/$GAME.adf
mkdir -p $D
python3 tools/make_icon.py tool $D/game.info
# AmigaDOS scripts: ; starts a comment; the device loader is harmless if the device is already resident
cat > $D/Start <<EOF
; SYN-ACK WIRE for the FujiNet-NIO firmware: load the FujiNet NIO device, then the game.
; From a Workbench 1.3 or later Shell or CLI:  Execute $VOL:Start
$VOL:nio/fujinet-load-resident $VOL:nio/fujinet-nio.device fujinet-nio.device
$VOL:$GAME
EOF
rm -f $ADF
xdftool $ADF format $VOL ofs >/dev/null
xdftool $ADF makedir nio >/dev/null
xdftool $ADF write build/amiga/$GAME $GAME >/dev/null
xdftool $ADF write $D/game.info $GAME.info >/dev/null
xdftool $ADF write $D/Start Start >/dev/null
xdftool $ADF write "$PKG/fujinet-load-resident" nio/fujinet-load-resident >/dev/null
xdftool $ADF write "$PKG/fujinet-nio.device" nio/fujinet-nio.device >/dev/null
xdftool $ADF list
echo "wrote $ADF"
