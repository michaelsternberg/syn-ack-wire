#!/bin/sh
# fnlib.sh SRC OUT -- build fujinet-lib (a checkout, SRC) for the Heathkit H89 under CP/M with zsdcc,
# the client's compiler: OUT/fujinet-h89.lib, the headers in OUT/include, the commit in OUT/VERSION.
# fujinet-lib's own h89-cpm target builds with sccz80, and no release has an H89 library yet.
#
# After FujiNet's M.U.L.E. client (fujinet-multiplayer-mule, clients/mekkogx/tools/z80/mkfnlib.sh, GPL-3.0),
# with its two H89 fixes to the copies:
#   - bus/h89/fuji_bus_call.c has a static copy of common/fuji_field.c's table, which fujinet-bus.h declares
#     extern: sccz80 lets it pass, zsdcc doesn't. The copy goes.
#   - network_open sends the URL with its NUL (NETWORK_OPEN_LEN is strlen + 1): the card runs the firmware's
#     rs232 code, which takes the string without a length and runs on into what its buffer held before.
set -e
src=$1
out=$2
[ -d "$src/bus/h89" ] || { echo "fnlib.sh: $src has no bus/h89" >&2; exit 1; }
rm -rf "$out"
mkdir -p "$out/src" "$out/obj" "$out/include"
cp "$src"/common/*.c "$src"/bus/h89/* "$out/src/"
cp -r "$src"/include/. "$out/include/"
git -C "$src" rev-parse HEAD > "$out/VERSION"
sed -i '/^static const uint8_t fuji_field_numbytes_table\[\]/d' "$out/src/fuji_bus_call.c"
sed -i 's/^#define NETWORK_OPEN_LEN strlen(devicespec)$/#define NETWORK_OPEN_LEN (strlen(devicespec) + 1)/' \
    "$out/include/fujinet-network.h"
grep -q 'strlen(devicespec) + 1' "$out/include/fujinet-network.h" || {
    echo "fnlib.sh: NETWORK_OPEN_LEN not patched" >&2; exit 1; }
sed -i '/#include <stdio.h>/d' "$out"/src/*.c
objs=
for c in "$out"/src/*.c; do
    o="$out/obj/$(basename "$c" .c).o"
    zcc +cpm -compiler=sdcc -SO3 -DBUILD_H89 -D__H89__ -I"$out/include" -I"$out/src" -c -o "$o" "$c"
    objs="$objs $o"
done
z88dk-z80asm -x"$out/fujinet-h89.lib" $objs
rm -rf "$out/src" "$out/obj"
echo "fnlib.sh: $out/fujinet-h89.lib ($(cut -c1-7 "$out/VERSION"))"
