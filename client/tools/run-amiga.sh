#!/bin/bash
# Run an Amiga program on an emulated stock A500 (68000, OCS, 512 KB chip, no other RAM) with Kickstart and
# Workbench 1.3, against a FujiNet NIO process, all on spare local ports.
#
#   tools/run-amiga.sh PROGRAM      start the rig and the program, return; its output is $RUN/share/log
#   tools/run-amiga.sh --stop       stop exactly the processes the rig started (the PIDs in $RUN/pids)
#
# The program is copied to the guest as SYNACK:game; SYNACK: is $RUN/share. The guest boots a copy of the
# Workbench 1.3 floppy whose Startup-Sequence loads the FujiNet broker and runs the program, output to
# SYNACK:log. Needs the fujinet-nio workspace (FUJINET_NIO_WS) built for Workbench 1.3:
# scripts/amiga-artifacts wb13, and its fujibus-tcp-debug/fujinet-nio. Licensed media paths come from
# AMIGA_ENV (default: the workspace's local/amiga.env). AMIGA_PREFS_SERVER=TCP://host:port/ writes that SERVER= line to the
# guest's S:SynAckWire.prefs before the game starts (the client's own server setting). Ports 65501 (NIO) and 23499 (Amiberry's serial port)
# must be free; the game server is yours, e.g. on 6599. Never kills by name: only recorded PIDs.
set -u
cd "$(dirname "$0")/.."
WS=${FUJINET_NIO_WS:-$HOME/dev/fujinet-nio-workspace}
RUN=${AMIGA_RUN_DIR:-build/amiga/run}
ENVF=${AMIGA_ENV:-$WS/local/amiga.env}
NIO_PORT=65501
SER_PORT=23499
die() { echo "run-amiga: $*" >&2; exit 1; }

stop() {
    [ -f "$RUN/pids" ] || { echo "nothing recorded to stop"; return 0; }
    while read -r name pid mark; do
        # a recorded PID counts only if its command line still holds our mark (a reused PID is not ours)
        if [ -r "/proc/$pid/cmdline" ] && tr '\0' ' ' < "/proc/$pid/cmdline" | grep -qF -- "$mark"; then
            kill "$pid" 2>/dev/null && echo "stopped $name ($pid)"
        fi
    done < "$RUN/pids"
    sleep 1
    mv "$RUN/pids" "$RUN/pids.stopped" 2>/dev/null
    return 0
}

[ "${1:-}" = "--stop" ] && { stop; exit 0; }
[ $# -eq 1 ] && [ -f "$1" ] || die "usage: run-amiga.sh PROGRAM | --stop"
PROG=$1

[ -f "$ENVF" ] || die "no $ENVF (licensed Kickstart and Workbench 1.3 paths: AMIGA_WB13_KICKSTART, AMIGA_WB13_ROM_KEY, AMIGA_WB13_ADF_WORKBENCH)"
. "$ENVF"
for v in AMIGA_WB13_KICKSTART AMIGA_WB13_ADF_WORKBENCH; do
    [ -f "${!v:-}" ] || die "$v is not set to a file in $ENVF"
done
PKG=$WS/build/amiga-artifacts/wb13/NIO
NIOBIN=$WS/repos/fujinet-nio/build/fujibus-tcp-debug/fujinet-nio
for f in "$PKG/fujinet-nio.device" "$PKG/fujinet-load-resident" "$NIOBIN"; do
    [ -f "$f" ] || die "$f is missing (in the workspace: scripts/amiga-artifacts wb13, and build fujinet-nio)"
done
command -v amiberry >/dev/null && command -v socat >/dev/null && command -v xdftool >/dev/null || die "needs amiberry, socat and xdftool"
for p in $NIO_PORT $SER_PORT; do
    ss -ltn | grep -q ":$p\b" && die "port $p is in use; not touching whatever holds it"
done
[ -f "$RUN/pids" ] && die "$RUN/pids exists: a rig is running (tools/run-amiga.sh --stop)"

mkdir -p "$RUN/share/nio" "$RUN/rom" "$RUN/nio/fujinet-data" || exit 1
RUN=$(cd "$RUN" && pwd)
rm -f "$RUN/share/log" "$RUN"/*.log
cp "$PROG" "$RUN/share/game"
cp "$PKG/fujinet-nio.device" "$PKG/fujinet-load-resident" "$RUN/share/nio/"
cp "$AMIGA_WB13_KICKSTART" "$RUN/rom/kickstart.rom"
[ -f "${AMIGA_WB13_ROM_KEY:-}" ] && cp "$AMIGA_WB13_ROM_KEY" "$RUN/rom/rom.key"
chmod u+w "$RUN"/rom/*

# the guest script, and a copy of the floppy whose boot runs it (the real start-up, minus Workbench and its
# 5-minute wait for StartupII, which a test has no use for)
cat > "$RUN/share/run.script" <<'GS'
; Kickstart 1.3 leaves a redirect in the program's arguments: the strict loader gets none, the game ignores it
SYNACK:nio/fujinet-load-resident SYNACK:nio/fujinet-nio.device fujinet-nio.device
GS
echo "SYNACK:game >SYNACK:log" >> "$RUN/share/run.script"
cp "$AMIGA_WB13_ADF_WORKBENCH" "$RUN/wb13.adf" && chmod u+w "$RUN/wb13.adf"
xdftool "$RUN/wb13.adf" read s/Startup-Sequence "$RUN/startup.orig" >/dev/null || die "xdftool could not read the floppy"
python3 - "$RUN" <<'PY' || exit 1
import sys, re
run = sys.argv[1]
s = open(run + "/startup.orig", "rb").read().decode("latin1")
cut = s.index("Failat 11")
open(run + "/startup.new", "wb").write((s[:cut] + "Path RAM: C: SYS:System S: SYS:Prefs ADD\nExecute SYNACK:run.script\n").encode("latin1"))
PY
if [ -n "${AMIGA_PREFS_SERVER:-}" ]; then
    printf 'SERVER=%s\n' "$AMIGA_PREFS_SERVER" > "$RUN/prefs.new"
    xdftool "$RUN/wb13.adf" write "$RUN/prefs.new" s/SynAckWire.prefs >/dev/null || die "xdftool could not write the prefs"
fi
xdftool "$RUN/wb13.adf" delete s/Startup-Sequence >/dev/null && xdftool "$RUN/wb13.adf" write "$RUN/startup.new" s/Startup-Sequence >/dev/null || die "xdftool could not write the floppy"

# NIO config: the workspace's shape, our port
cat > "$RUN/nio/fujinet-data/fujinet.yaml" <<YML
fujinet:
  device_name: "synack-amiga"
boot:
  mode: config
  config_uri: "persist:/boot/autorun.img"
  readonly: true
wifi:
  enabled: false
  ssid: ""
  passphrase: ""
devices:
  modem:
    enabled: false
    sniffer_enabled: false
  cpm:
    enabled: false
    ccp_image: ""
  printer:
    enabled: false
netsio:
  enabled: false
  host: "localhost"
  port: 9997
clock:
  timezone: "UTC"
  enabled: true
channel:
  pty_path: ""
  tcp_host: "127.0.0.1"
  tcp_port: $NIO_PORT
  serial_port: "/dev/ttyUSB0"
  uart:
    baud_rate: 115200
    data_bits: 8
    parity: none
    stop_bits: 1
    flow_control: none
    tx_gap_us: 0
YML

record() { echo "$1 $2 $3" >> "$RUN/pids"; }
( cd "$RUN/nio" && exec "$NIOBIN" ) > "$RUN/nio.log" 2>&1 &
record nio $! fujinet-nio
for i in $(seq 1 20); do ss -ltn | grep -q ":$NIO_PORT\b" && break; sleep 0.5; done
ss -ltn | grep -q ":$NIO_PORT\b" || { stop; die "fujinet-nio did not listen on $NIO_PORT (see $RUN/nio.log)"; }

( exec amiberry --log --config "$WS/configs/amiga/base-68000.uae" -G -r "$RUN/rom/kickstart.rom" -0 "$RUN/wb13.adf" \
    -s chipset=ocs -s ntsc=${AMIGA_NTSC:-false} -s chipmem_size=1 -s fastmem_size=0 -s bogomem_size=0 \
    -s serial_port=tcp://127.0.0.1:$SER_PORT -s serial_direct=true -s serial_hardware_ctsrts=false \
    -s "filesystem2=rw,DH1:SYNACK:$RUN/share,0" ) > "$RUN/amiberry.log" 2>&1 &
record amiberry $! "$RUN/rom/kickstart.rom"
for i in $(seq 1 120); do grep -q "TCP: Listening" "$RUN/amiberry.log" && break; sleep 0.5; done
grep -q "TCP: Listening" "$RUN/amiberry.log" || { stop; die "amiberry did not open its serial port (see $RUN/amiberry.log)"; }

( exec socat TCP:127.0.0.1:$SER_PORT TCP:127.0.0.1:$NIO_PORT ) > "$RUN/socat.log" 2>&1 &
record socat $! "TCP:127.0.0.1:$SER_PORT"
echo "rig running; guest output: $RUN/share/log   stop with: tools/run-amiga.sh --stop"
