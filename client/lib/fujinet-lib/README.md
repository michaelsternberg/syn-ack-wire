# fujinet-lib, pinned

The FujiNet library the Apple II, MS-DOS, CoCo, NES and Adam clients use for their network, AppKey and clock calls
(`src/apple2/net_fn.c`, `src/apple2/input.c`; `src/msdos/`; `src/coco/net_fn.c`; `src/nes/net_fn.c`; `src/adam/net_fn.c`). A copy lives here, unchanged, so a change upstream
can't break the build. The Atari client doesn't use it (it talks raw SIO: `src/atari/net_sio.s`).

| | Apple II | MS-DOS | CoCo | NES | Adam |
|---|---|---|---|---|---|
| Version | 5.0.0 (2026-10-16) | 5.0.0 (2026-10-16) | 5.0.0 (2026-10-16) | not released: commit `a29e57f` (2026-10-04) | the NES's commit, `a29e57f` |
| From | https://github.com/FujiNetWIFI/fujinet-lib/releases/tag/v5.0.0, `fujinet-lib-apple2-5.0.0.zip` | the same release, `fujinet-lib-msdos-5.0.0.zip` | the same release, `fujinet-lib-coco-5.0.0.zip` | branch `add-nes` of https://github.com/tschak909/fujinet-lib-experimental, built here (below) | the same commit, built here with zsdcc (below) |
| sha256 of the zip | `c87d1ba1706ba332237127935ca5572dd4114eb38bd447f691a525a0c386532c` | `182f1ad1826f0a6482f14fa5cbe972fb44f3dd6469584f1c5e377b5526a05b27` | `75151d41364815832feae3087ad4a944634b526212e77a1b59277a6f7835f152` | of the `.lib`: `a27ab3e4d4d69966021e444474d640c3e794f415ca7e9af3ae78aa1a539b0686` | of the `.lib`: `ae0ad533ba79ddb8156f6f8a01b84f4b64609c5024194fdefeb81816cf3b152d` |
| Licence | GPL-3.0, as this project | GPL-3.0, as this project | GPL-3.0, as this project | GPL-3.0, as fujinet-lib (the fork carries no `LICENSE` file of its own) | the same |
| Built with | the release's own cc65; links with cc65 V2.19 (Git 8d86ca7) | Open Watcom v2 (`wcc -0 -bt=dos`: 8086 code); calls FujiNet through `FUJINET.SYS`'s INT F5h | CMOC; links with CMOC 0.1.100 and lwtools 4.25; DriveWire, over the serial port or a Becker port | cc65 V2.19 (Git 92fa7cb23); the FujiNet NES cartridge's mailbox at $5000-$57FF | z88dk v25230-82c677065d (zsdcc, in `fozztexx/defoogi:1.4.7`); AdamNet, through EOS's DCBs |

`apple2-5.0.0/` holds the zip's contents: `fujinet-apple2-5.0.0.lib`, its `Changelog.md`, and the headers in
`include/`. The linker takes only the modules the client calls: about 3.4 KB of the Apple client's 15 KB
(the general bus layer is 1.5 KB of that, linked whole). `msdos-5.0.0/` is laid out the same way, with
`fujinet-msdos-5.0.0.lib`; its zip has the files at the top level, so the headers were moved into `include/`. `coco-5.0.0/` likewise:
`fujinet-coco-5.0.0.lib`, `Changelog.md`, and the headers (and `fujinet-err.inc`) moved into `include/`.

`nes-a29e57f/` is no release: the NES target exists only on a branch of a fork. It was built from a clean
export of that commit, not from a working copy, so nothing uncommitted got in:
```sh
git -C fujinet-lib-experimental archive a29e57f | tar -x -C fnlib && make -C fnlib nes   # -> r2r/nes/fujinet.nes.lib
```
It holds that library as `fujinet-nes-a29e57f.lib`, the headers in `include/` (`fujinet-bus-nes.h`, the mailbox's
addresses, copied there from `bus/nes/`), and `nes-romstamp.py` from `makefiles/`, which the build runs to write the
`FUJI` claim at $FFF0 (without it the cartridge shuts its mailbox off once the game boots) and to check that no
read-modify-write instruction touches the mailbox's write-only pages.

`adam-a29e57f/` is the same commit built for the Adam with zsdcc, the client's compiler: fujinet-lib's released Adam
library is an sccz80 build, which takes its arguments in the other order. `tools/fnlib_adam.sh` exports the commit
(`git archive`), compiles `common/*.c` and `bus/adam/*` with `zcc +coleco -subtype=adam -compiler=sdcc -SO3`, as
fujinet-multiplayer-mule's `tools/z80/mkfnlib.sh` does, less the `<stdio.h>` its debug lines include, and
archives them with `z80asm`:
```sh
tools/fnlib_adam.sh ~/Workspace/fujinet-lib-experimental a29e57f "$PWD/build/fnlib-adam"   # from client/
```
It holds that library as `fujinet-adam-a29e57f.lib` and the headers in `include/` (`fujinet-bus-adam.h` copied
there from `bus/adam/`). The objects record their source paths, so the same `.lib` comes only from that same
folder. The client uses it for the open, the close and the AppKeys; the stream between them drives the network
device's DCB itself (`src/adam/net_fn.c`).

The H89 (`h89-194824b/`) is the exception: no release has an H89 library yet, so it is built from source,
fujinet-lib's `master` at commit `194824ba1fb6713752cbc0af9e14f6febc59b6fa` (2026-10-28), by
`tools/h89/fnlib.sh SRC h89-194824b` with z88dk's zsdcc, the client's compiler (fujinet-lib's own `h89-cpm`
target uses sccz80, and the two don't mix). The script makes two fixes to its copies, both from FujiNet's
M.U.L.E. client: a duplicate table in `bus/h89/fuji_bus_call.c` goes, and `network_open` sends the URL with
its NUL. The folder holds `fujinet-h89.lib`, the headers in `include/`, and the commit in `VERSION`.
When a release has the H89, it moves to that like the others.

To move to another version: download its release zip for each platform used, check the sha256 against the
release page, unpack it into a new `<platform>-<version>/` folder beside the old one, point `A2LIB` (or the
MS-DOS, CoCo or NES build's library path) in `client/Makefile` at it, build, play a game, then delete the old folder and
update this table.
