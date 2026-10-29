#!/bin/sh
# Deploy to fly.io, but only while no one is playing: a deploy restarts the machine and ends every game.
#
#   deploy/deploy.sh            check, then fly deploy --ha=false if all is quiet
#   deploy/deploy.sh --check    check only
#   deploy/deploy.sh --force    deploy without checking
#
# Checks, from the repository root's fly.toml and server/servers.fly.toml:
#   1. the client disks the image serves exist: client/build/atari/syn-ack-wire-atari.atr,
#      client/build/apple2_text/syn-ack-wire-apple2.po, client/build/msdos/syn-ack-wire-msdos.img,
#      client/build/coco/syn-ack-wire-coco.dsk, client/build/nes/syn-ack-wire-nes.nes and
#      client/build/adam/syn-ack-wire-adam.ddp
#   2. the FujiNet lobby (LOBBY, default the public one) shows no players on any of this app's servers: humans in
#      a server's lobby, or seats humans hold in a game, dropped players included; if it can't be read, stop
#   3. fly logs show no game activity in the last QUIET minutes (default 10): someone may be at a title screen
set -eu
cd "$(dirname "$0")/.."

mode=deploy
case "${1:-}" in
    --check) mode=check ;;
    --force) mode=force ;;
    "") ;;
    *) echo "usage: deploy/deploy.sh [--check | --force]" >&2; exit 2 ;;
esac

app=$(sed -n 's/^app *= *"\([^"]*\)".*/\1/p' fly.toml)
[ -n "$app" ] || { echo "fly.toml: no app name" >&2; exit 1; }

if [ "$mode" != force ]; then
    for f in client/build/atari/syn-ack-wire-atari.atr client/build/apple2_text/syn-ack-wire-apple2.po \
             client/build/msdos/syn-ack-wire-msdos.img client/build/coco/syn-ack-wire-coco.dsk \
             client/build/nes/syn-ack-wire-nes.nes client/build/adam/syn-ack-wire-adam.ddp; do
        [ -f "$f" ] || { echo "$f is missing: build it first (server/README.md, Deploying to fly.io)" >&2; exit 1; }
    done

    echo "players, from the lobby:"
    curl -fsS --max-time 20 "${LOBBY:-https://lobby.fujinet.online}/viewFull" | python3 -c '
import json, sys, tomllib
address = tomllib.load(open("server/servers.fly.toml", "rb"))["lobby"]["address"]
mine = [s for s in json.load(sys.stdin) if f"://{address}:".lower() in s["serverurl"].lower()]
if not mine:
    sys.exit(f"  none of the servers at {address} is in the lobby: is the app running?")
for s in mine:
    print("  %-32s %d of %d" % (s["server"], s["curplayers"], s["maxplayers"]))
if any(s["curplayers"] for s in mine):
    print("someone is playing: not deploying")
    sys.exit(1)
' || { echo "stopped (deploy/deploy.sh --force deploys anyway)" >&2; exit 1; }

    echo "game activity in the last ${QUIET:-10} minutes, from fly logs:"
    fly logs -a "$app" --no-tail 2>/dev/null | QUIET="${QUIET:-10}" python3 -c '
import os, re, sys
from datetime import datetime, timedelta, timezone
since = datetime.now(timezone.utc) - timedelta(minutes=int(os.environ["QUIET"]))
recent = []
for line in sys.stdin:
    line = re.sub(r"\x1b\[[0-9;]*m", "", line).rstrip()
    m = re.match(r"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ) .*?\] ?\d{4}-\d\d-\d\d \S+ (.*)", line)
    if not m:
        continue                                    # tnfsd, the proxy: not games
    when = datetime.strptime(m.group(1), "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    if when >= since and re.search(r"table \d+:|joined|disconnected", m.group(2)) \
            and not re.search(r"abandoned|listening|lobby:", m.group(2)):
        recent.append(m.group(1)[11:19] + "  " + m.group(2))
for r in recent[-10:]:
    print("  " + r)
if recent:
    print(f"{len(recent)} lines of game activity (the last 10 above): not deploying (QUIET=0 skips this check)")
    sys.exit(1)
print("  none")
' || { echo "stopped (deploy/deploy.sh --force deploys anyway)" >&2; exit 1; }
fi

[ "$mode" = check ] && { echo "all quiet: deploy/deploy.sh would deploy"; exit 0; }
exec fly deploy --ha=false
