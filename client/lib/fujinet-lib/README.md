# fujinet-lib, pinned

The FujiNet library the Apple II, MS-DOS, CoCo and NES clients use for their network, AppKey and clock calls
(`src/apple2/net_fn.c`, `src/apple2/input.c`; `src/msdos/`; `src/coco/net_fn.c`; `src/nes/net_fn.c`). A copy lives here, unchanged, so a change upstream
can't break the build. The Atari client doesn't use it (it talks raw SIO: `src/atari/net_sio.s`).

| | Apple II | MS-DOS | CoCo | NES |
|---|---|---|---|---|
| Version | 5.0.0 (2026-10-16) | 5.0.0 (2026-10-16) | 5.0.0 (2026-10-16) | not released: commit `a29e57f` (2026-10-04) |
| From | https://github.com/FujiNetWIFI/fujinet-lib/releases/tag/v5.0.0, `fujinet-lib-apple2-5.0.0.zip` | the same release, `fujinet-lib-msdos-5.0.0.zip` | the same release, `fujinet-lib-coco-5.0.0.zip` | branch `add-nes` of https://github.com/tschak909/fujinet-lib-experimental, built here (below) |
| sha256 of the zip | `c87d1ba1706ba332237127935ca5572dd4114eb38bd447f691a525a0c386532c` | `182f1ad1826f0a6482f14fa5cbe972fb44f3dd6469584f1c5e377b5526a05b27` | `75151d41364815832feae3087ad4a944634b526212e77a1b59277a6f7835f152` | of the `.lib`: `a27ab3e4d4d69966021e444474d640c3e794f415ca7e9af3ae78aa1a539b0686` |
| Licence | GPL-3.0, as this project | GPL-3.0, as this project | GPL-3.0, as this project | GPL-3.0, as fujinet-lib (the fork carries no `LICENSE` file of its own) |
| Built with | the release's own cc65; links with cc65 V2.19 (Git 8d86ca7) | Open Watcom v2 (`wcc -0 -bt=dos`: 8086 code); calls FujiNet through `FUJINET.SYS`'s INT F5h | CMOC; links with CMOC 0.1.100 and lwtools 4.25; DriveWire, over the serial port or a Becker port | cc65 V2.19 (Git 92fa7cb23); the FujiNet NES cartridge's mailbox at $5000-$57FF |

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

To move to another version: download its release zip for each platform used, check the sha256 against the
release page, unpack it into a new `<platform>-<version>/` folder beside the old one, point `A2LIB` (or the
MS-DOS, CoCo or NES build's library path) in `client/Makefile` at it, build, play a game, then delete the old folder and
update this table.
