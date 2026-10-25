#!/bin/sh
# Play the MS-DOS client in a DOSBox-X window, with FujiNet-PC (built for RS-232) on its COM1, as run-test.sh
# does for the Atari. DOSBox-X boots your own PC-DOS disk image (DOSDISK) with FUJINET.SYS in CONFIG.SYS, the
# game on A:, and SYNACK.HLP beside it. COM1 is a null modem over TCP to FujiNet-PC's BoIP port. Your DOSBox-X
# settings and fnconfig.ini are not touched: DOSBox-X runs on its built-in defaults, FujiNet-PC on a scratch copy.
#
# EMU=mame runs the same on MAME's IBM PC 5150 instead: an 8088 at 4.77 MHz, a CGA card (CARD=mda: a monochrome
# one), an AdLib card with ADLIB=1. It needs the ibm5150 ROMs (ROMS=folder). Its memory test takes a minute before
# DOS boots. MAME's own settings go in the scratch folder. If a function key works MAME rather than the PC, Scroll
# Lock switches the keyboard between the two.
#
#   tools/run-dos.sh                              build/msdos/SYNACK.EXE (make msdos URL=... first)
#   tools/run-dos.sh build/msdos/local/SYNACK.EXE
#   DOSDISK=disk.img      a bootable 360 KB PC-DOS disk image (default: the owner's PC-DOS 3.30 disk 1)
#   FUJINET_DIR=dir       FujiNet-PC's dist folder, built with FUJINET_TARGET=RS232
#   FUJINET_SYS=file      fujinet.sys from fujinet-rs232 (sys/), built for the same firmware
#   BOIP=65504 WEB=8003   the ports FujiNet-PC listens on (BoIP) and serves its web page on
#
# Start the server first:  cd ../server && python3 serve.py
# Closing the DOSBox-X window stops FujiNet-PC as well.
set -e
cd "$(dirname "$0")/.."
EXE=$(realpath "${1:-build/msdos/SYNACK.EXE}")
HLP=$(dirname "$EXE")/SYNACK.HLP
FNW=$HOME/Nextcloud/Projects/computers/atari8/Source/FujiNetWIFI
FUJINET_DIR=${FUJINET_DIR:-$FNW/fujinet-firmware/build-rs232/dist}
FUJINET_SYS=${FUJINET_SYS:-$FNW/fujinet-rs232/sys/fujinet.sys}
DOSDISK=${DOSDISK:-$HOME/Nextcloud/Projects/computers/ibm-5150/software/dos/IBM PC-DOS 3.30 (5.25)/Ibm330_1.img}
BOIP=${BOIP:-65504}
WEB=${WEB:-8003}
ROMS=${ROMS:-$HOME/Nextcloud/Projects/mame/roms}

for p in $BOIP $WEB; do
    if ss -ltn | grep -q ":$p\b"; then echo "port $p is in use: pick another BOIP or WEB" >&2; exit 1; fi
done
WORK=$(mktemp -d "${TMPDIR:-/tmp}/synack-dos.XXXXXX")

# the boot disk: a copy of DOSDISK with room made (files a game disk doesn't need), the driver and the game
cp "$DOSDISK" "$WORK/a.img"
for f in EGA.CPI FDISK.COM 4201.CPI PRINTER.SYS KEYBOARD.SYS BASICA.COM; do mdel -i "$WORK/a.img" ::$f 2>/dev/null || true; done
printf 'DEVICE=FUJINET.SYS\r\n' > "$WORK/CONFIG.SYS"
printf '@ECHO OFF\r\nSYNACK\r\n' > "$WORK/AUTOEXEC.BAT"
mcopy -i "$WORK/a.img" "$FUJINET_SYS" ::FUJINET.SYS
mcopy -i "$WORK/a.img" "$WORK/CONFIG.SYS" "$WORK/AUTOEXEC.BAT" ::
mcopy -i "$WORK/a.img" "$EXE" ::SYNACK.EXE
[ -f "$HLP" ] && mcopy -i "$WORK/a.img" "$HLP" ::SYNACK.HLP

# FujiNet-PC: a scratch copy of its settings, listening for BoIP on our port
awk -v port=$BOIP '/^\[/ { boip = /^\[BOIP\]/ }
     boip && /^enabled=/ { $0 = "enabled=1" }
     boip && /^port=/ { $0 = "port=" port }
     boip && /^host=/ { $0 = "host=127.0.0.1" } 1' "$FUJINET_DIR/fnconfig.ini" > "$WORK/fnconfig.ini"
mkdir "$WORK/sd"
(cd "$FUJINET_DIR" && exec ./fujinet -c "$WORK/fnconfig.ini" -s "$WORK/sd" -u "http://127.0.0.1:$WEB") \
    > "$WORK/fujinet.log" 2>&1 &
FN=$!
trap 'kill $FN 2>/dev/null; echo "logs: $WORK"' EXIT INT TERM
sleep 1

if [ "$EMU" = mame ]; then
    # COM1's null modem at 115200, as FUJINET.SYS expects, before DOS boots
    printf '%s\n' 'for _, p in ipairs({ "RS232_TXBAUD", "RS232_RXBAUD" }) do' \
        '  for _, f in pairs(manager.machine.ioport.ports[":isa2:com:serport0:null_modem:" .. p].fields) do' \
        '    f.user_value = 13' '  end' 'end' > "$WORK/baud.lua"
    if [ "$CARD" = mda ]; then      # the motherboard's switches must say monochrome, or the BIOS fails its video check
        printf '%s\n' 'local f = manager.machine.ioport.ports[":mb:DSW0"].fields["Graphics adapter"]' \
            'if f.user_value ~= 48 then f.user_value = 48; manager.machine:soft_reset() end' >> "$WORK/baud.lua"
    fi
    cd "$WORK" && mame -rompath "$ROMS" ibm5150 -isa1 "${CARD:-cga}" -isa2:com:serport0 null_modem \
        -bitb "socket.127.0.0.1:$BOIP" ${ADLIB:+-isa4 adlib} -flop1 "$WORK/a.img" -window -skip_gameinfo \
        -autoboot_script "$WORK/baud.lua" -autoboot_delay 0
else
    dosbox-x -defaultconf -nopromptfolder \
        -set "serial serial1=nullmodem server:127.0.0.1 port:$BOIP transparent:1" \
        -c "imgmount a $WORK/a.img -t floppy" -c "boot -l a"
fi
